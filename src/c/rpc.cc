//- Copyright 2015 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "stdc.h"

#include "async/promise-inl.hh"
#include "marshal-inl.hh"
#include "rpc.hh"

BEGIN_C_INCLUDES
#include "utils/log.h"
END_C_INCLUDES

using namespace plankton;
using namespace tclib;

namespace plankton {

// The data associated with an RPC request that goes on the wire. It's basically
// a plain Request with a bit of extra data added.
class RequestMessage {
public:
  RequestMessage()
    : serial_(0) { }
  RequestMessage(Request *request, uint64_t serial)
    : request_(*request)
    , serial_(serial) { }
  static SeedType<RequestMessage> *seed_type() { return &kSeedType; }
  Request &request() { return request_; }
  uint64_t serial() { return serial_; }
public:
  Variant to_seed(Factory *factory);
  static RequestMessage *new_instance(Variant header, Factory *factory);
  void init(Seed payload, Factory *factory);
  static SeedType<RequestMessage> kSeedType;
  Request request_;
  uint64_t serial_;
};

}

SeedType<RequestMessage> RequestMessage::kSeedType("rpc.Request",
    RequestMessage::new_instance,
    new_callback(&RequestMessage::init),
    new_callback(&RequestMessage::to_seed));

RequestMessage *RequestMessage::new_instance(Variant header, Factory *factory) {
  return new (factory) RequestMessage();
}

void RequestMessage::init(Seed payload, Factory *factory) {
  serial_ = payload.get_field("serial").integer_value();
  request().set_subject(payload.get_field("subject"));
  request().set_selector(payload.get_field("selector"));
  request().set_arguments(payload.get_field("arguments"));
}

Variant RequestMessage::to_seed(Factory *factory) {
  Seed result = factory->new_seed(seed_type());
  result.set_field("serial", serial_);
  result.set_field("subject", request().subject());
  result.set_field("selector", request().selector());
  result.set_field("arguments", request().arguments());
  return result;
}

Request::Request(Variant subject, Variant selector, size_t argc, Variant *argv)
  : subject_(subject)
  , selector_(selector) {
  if (argc == 0)
    return;
  arguments_ = arena_.new_map();
  for (size_t i = 0; i < argc; i++)
    arguments_.map_set(i, argv[i]);
}

namespace plankton {

// The data for an RPC response that goes on the wire. It's basically a plain
// OutgoingResponse with a bit of extra data added.
class ResponseMessage {
public:
  ResponseMessage()
    : serial_(0) { }
  ResponseMessage(OutgoingResponse *response, uint64_t serial)
    : response_(*response)
    , serial_(serial) { }
  uint64_t serial() { return serial_; }
  OutgoingResponse &response() { return response_; }
  static SeedType<ResponseMessage> *seed_type() { return &kSeedType; }
private:
  Variant to_seed(Factory *factory);
  static ResponseMessage *new_instance(Variant header, Factory *factory);
  void init(Seed payload, Factory *factory);
  static SeedType<ResponseMessage> kSeedType;
  OutgoingResponse response_;
  uint64_t serial_;
};

}

SeedType<ResponseMessage> ResponseMessage::kSeedType("rpc.Response",
    ResponseMessage::new_instance,
    new_callback(&ResponseMessage::init),
    new_callback(&ResponseMessage::to_seed));

ResponseMessage *ResponseMessage::new_instance(Variant header, Factory *factory) {
  return new (factory) ResponseMessage();
}

void ResponseMessage::init(Seed payload, Factory *factory) {
  serial_ = payload.get_field("serial").integer_value();
  OutgoingResponse::Status status = payload.get_field("is_success").bool_value()
      ? OutgoingResponse::SUCCESS
      : OutgoingResponse::FAILURE;
  response_ = OutgoingResponse(status, payload.get_field("payload"));
}

Variant ResponseMessage::to_seed(Factory *factory) {
  Seed result = factory->new_seed(seed_type());
  result.set_field("serial", serial_);
  result.set_field("is_success", Variant::boolean(response().is_success()));
  result.set_field("payload", response().payload());
  return result;
}

class MessageSocket::PendingMessage : public IncomingResponse {
public:
  PendingMessage(uint64_t serial);
  virtual ~PendingMessage() { }
  virtual sync_promise_t<Variant, Variant> *operator->() { return &promise_; }
private:
  friend class MessageSocket;
  Arena arena_;
  uint64_t serial_;
  sync_promise_t<Variant, Variant> promise_;
};

MessageSocket::MessageSocket(PushInputStream *in, OutputSocket *out, RequestHandler handler)
  : in_(NULL)
  , out_(NULL)
  , next_serial_(1) {
  init(in, out, handler);
}

MessageSocket::MessageSocket()
  : in_(NULL)
  , out_(NULL)
  , next_serial_(1) { }

void MessageSocket::init(PushInputStream *in, OutputSocket *out, RequestHandler handler) {
  in_ = in;
  out_ = out;
  handler_ = handler;
  types_.register_type(RequestMessage::seed_type());
  types_.register_type(ResponseMessage::seed_type());
  in_->set_type_registry(&types_);
  in_->add_action(tclib::new_callback(&MessageSocket::on_incoming_message, this));
}

void MessageSocket::on_incoming_message(ParsedMessage *message) {
  Variant value = message->value();
  RequestMessage *request = value.native_as(RequestMessage::seed_type());
  if (request != NULL) {
    on_incoming_request(request);
    return;
  }
  ResponseMessage *response = value.native_as(ResponseMessage::seed_type());
  if (response != NULL) {
    on_incoming_response(message->owner(), response);
    return;
  }
  // An unexpected message. Log but ignore.
  TextWriter writer;
  writer.write(value);
  WARN("Unexpected incoming message: %s", *writer);
}

void MessageSocket::on_incoming_request(RequestMessage *message) {
  Request *request = &message->request();
  uint64_t serial = message->serial();
  (handler_)(request, new_callback(&MessageSocket::on_outgoing_response,
      this, serial));
}

void MessageSocket::on_incoming_response(VariantOwner *owner, ResponseMessage *message) {
  uint64_t serial = message->serial();
  PendingMessageMap::iterator pendings = pending_messages_.find(serial);
  if (pendings == pending_messages_.end()) {
    // This response is out of band; ignore.
    WARN("Incoming response out of band: serial %i", serial);
    return;
  }
  PendingMessage *pending = pendings->second;
  // The response is currently owned by a transient arena somewhere else. Since
  // we're going to be storing it indefinitely in the pending message the
  // message needs to adopt ownership of that arena and hence the whole message.
  pending->arena_.adopt_ownership(owner);
  OutgoingResponse &response = message->response();
  if (response.is_success()) {
    pending->promise().fulfill(response.payload());
  } else {
    pending->promise().fail(response.payload());
  }
  pending_messages_.erase(serial);
}

void MessageSocket::on_outgoing_response(uint64_t serial, OutgoingResponse *response) {
  ResponseMessage message(response, serial);
  Arena arena;
  Native value = arena.new_native(&message);
  out_->send_value(value);
}

MessageSocket::PendingMessage::PendingMessage(uint64_t serial)
  : serial_(serial)
  , promise_(sync_promise_t<Variant, Variant>::empty()) { }

IncomingResponse* MessageSocket::send_request(Request *request) {
  Arena arena;
  uint64_t serial = next_serial_++;
  RequestMessage message(request, serial);
  PendingMessage *pending = new PendingMessage(serial);
  pending_messages_[serial] = pending;
  Native wrapped = arena.new_native(&message);
  out_->send_value(wrapped);
  return pending;
}

OutgoingResponse::OutgoingResponse()
  : status_(SUCCESS) { }

OutgoingResponse::OutgoingResponse(Status status, Variant payload)
  : status_(status)
  , payload_(payload) { }

Service::Service()
  : handler_(new_callback(&Service::on_request, this)) { }

void Service::register_method(Variant selector, MethodZero handler) {
  methods_.set(selector, tclib::new_callback(method_zero_trampoline, handler));
}

void Service::register_method(Variant selector, MethodOne handler) {
  methods_.set(selector, tclib::new_callback(method_one_trampoline, handler));
}

void Service::on_request(Request* request, ResponseCallback response) {
  GenericMethod *method = methods_[request->selector()];
  if (method == NULL) {
    OutgoingResponse reply(OutgoingResponse::FAILURE, Variant::null());
    response(&reply);
  } else {
    (*method)(request->arguments(), response);
  }
}

void Service::method_zero_trampoline(MethodZero delegate, Variant args,
    ResponseCallback callback) {
  delegate(callback);
}

void Service::method_one_trampoline(MethodOne delegate, Variant args,
    ResponseCallback callback) {
  delegate(args.map_get(Variant::integer(0)), callback);
}
