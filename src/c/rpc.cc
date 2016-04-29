//- Copyright 2015 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "c/stdc.h"

#include "async/promise-inl.hh"
#include "marshal-inl.hh"
#include "rpc.hh"

BEGIN_C_INCLUDES
#include "utils/log.h"
END_C_INCLUDES

using namespace plankton;
using namespace plankton::rpc;
using namespace plankton::rpc::internal;
using namespace tclib;

namespace plankton {
namespace rpc {
namespace internal {

// The data associated with an RPC request that goes on the wire. It's basically
// a plain Request with a bit of extra data added.
class RequestMessage {
public:
  RequestMessage()
    : serial_(0) { }
  RequestMessage(OutgoingRequest *request, uint64_t serial)
    : request_(*request)
    , serial_(serial) { }
  static SeedType<RequestMessage> *seed_type() { return &kSeedType; }
  OutgoingRequest &request() { return request_; }
  uint64_t serial() { return serial_; }
public:
  Variant to_seed(Factory *factory);
  static RequestMessage *new_instance(Variant header, Factory *factory);
  void init(Seed payload, Factory *factory);
  static SeedType<RequestMessage> kSeedType;
  OutgoingRequest request_;
  uint64_t serial_;
};

}
}
}

SeedType<RequestMessage> RequestMessage::kSeedType("rpc.Request",
    RequestMessage::new_instance,
    new_callback(&RequestMessage::init),
    new_callback(&RequestMessage::to_seed));

RequestMessage *RequestMessage::new_instance(Variant header, Factory *factory) {
  return factory->register_destructor(new (factory) RequestMessage());
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

OutgoingRequest::OutgoingRequest(Variant subject, Variant selector, size_t argc,
    Variant *argv)
  : subject_(subject)
  , selector_(selector)
  , arguments_(Variant::null()) {
  set_arguments(argc, argv);
}

void OutgoingRequest::set_arguments(size_t argc, Variant *argv) {
  for (size_t i = 0; i < argc; i++)
    set_argument(i, argv[i]);
}

void OutgoingRequest::set_argument(Variant key, Variant value) {
  if (arguments_.is_null())
    arguments_ = arena_.new_map();
  arguments_.map_set(key, value);
}

MessageSocketObserver::MessageSocketObserver()
  : subject_(NULL)
  , next_(NULL)
  , is_installed_(false) {
}

MessageSocketObserver::~MessageSocketObserver() {
  if (is_installed_)
    uninstall();
}

void MessageSocketObserver::install(MessageSocket *subject) {
  CHECK_FALSE("already installed", is_installed_);
  subject_ = subject;
  next_ = subject->observer();
  subject->observer_ = this;
  is_installed_ = true;
}

void MessageSocketObserver::uninstall() {
  CHECK_TRUE("not installed", is_installed_);
  CHECK_PTREQ("observer out of order", subject()->observer(), this);
  subject()->observer_ = next();
  next_ = NULL;
  is_installed_ = false;
}

void MessageSocketObserver::notify_incoming_request(IncomingRequest *request,
    uint64_t serial) {
  on_incoming_request(request, serial);
  if (next() != NULL)
    next()->notify_incoming_request(request, serial);
}

void MessageSocketObserver::notify_outgoing_response(OutgoingResponse response,
    uint64_t serial) {
  on_outgoing_response(response, serial);
  if (next() != NULL)
    next()->notify_outgoing_response(response, serial);
}

TracingMessageSocketObserver::TracingMessageSocketObserver(const char *prefix, OutStream *out)
  : prefix_(prefix)
  , out_(out) {
  if (out == NULL)
    out_ = FileSystem::native()->std_out();
}

void TracingMessageSocketObserver::on_incoming_request(rpc::IncomingRequest *request,
      uint64_t serial) {
  TextWriter selw;
  selw.write(request->selector());
  TextWriter argw;
  argw.write(request->arguments());
  out()->printf("%s <%i| %s %s\n", prefix_, static_cast<uint32_t>(serial),
      *selw, *argw);
  out()->flush();
}

void TracingMessageSocketObserver::on_outgoing_response(rpc::OutgoingResponse response,
      uint64_t serial) {
  TextWriter payw;
  payw.write(response.payload());
  const char *indicator = response.is_success() ? "|" : "!";
  out()->printf("%s %s%i> %s\n", prefix_, indicator, static_cast<uint32_t>(serial),
      *payw);
  out()->flush();
}

namespace plankton {
namespace rpc {
namespace internal {

// The data for an RPC response that goes on the wire. It's basically a plain
// OutgoingResponse with a bit of extra data added.
class ResponseMessage {
public:
  ResponseMessage()
    : serial_(0) { }
  ResponseMessage(OutgoingResponse response, uint64_t serial)
    : response_(response)
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
}
}

SeedType<ResponseMessage> ResponseMessage::kSeedType("rpc.Response",
    ResponseMessage::new_instance,
    new_callback(&ResponseMessage::init),
    new_callback(&ResponseMessage::to_seed));

ResponseMessage *ResponseMessage::new_instance(Variant header, Factory *factory) {
  return factory->register_destructor(new (factory) ResponseMessage());
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

size_t MessageSocket::SerialHasher::operator()(const Serial &key) const {
  return static_cast<size_t>(key.value_);
}

bool MessageSocket::SerialHasher::operator()(const Serial &a, const Serial &b) {
  return a.value_ < b.value_;
}

class MessageSocket::PendingMessage : public internal::IncomingResponseData {
public:
  PendingMessage();
  virtual ~PendingMessage() { }
  virtual sync_promise_t<Variant, Variant> *result() { return &promise_; }

protected:
  virtual size_t instance_size() { return sizeof(*this); }

private:
  friend class MessageSocket;
  Arena arena_;
  sync_promise_t<Variant, Variant> promise_;
};

MessageSocket::MessageSocket(PushInputStream *in, OutputSocket *out,
    RequestCallback handler)
  : in_(NULL)
  , out_(NULL)
  , next_serial_(1) {
  init(in, out, handler);
}

MessageSocket::MessageSocket()
  : in_(NULL)
  , out_(NULL)
  , next_serial_(1)
  , observer_(NULL) { }

fat_bool_t MessageSocket::init(PushInputStream *in, OutputSocket *out,
    RequestCallback handler) {
  in_ = in;
  out_ = out;
  handler_ = handler;
  types_.add_fallback(in->type_registry());
  types_.register_type(RequestMessage::seed_type());
  types_.register_type(ResponseMessage::seed_type());
  in_->set_type_registry(&types_);
  in_->add_action(tclib::new_callback(&MessageSocket::on_incoming_message, this));
  if (!out_guard_.initialize())
    return F_FALSE;
  return F_TRUE;
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
  IncomingRequest request(&message->request());
  uint64_t serial = message->serial();
  if (observer() != NULL)
    observer()->notify_incoming_request(&request, serial);
  ResponseCallback callback = new_callback(&MessageSocket::on_outgoing_response,
      this, serial);
  (handler_)(&request, callback);
}

void MessageSocket::on_incoming_response(VariantOwner *owner, ResponseMessage *message) {
  Serial serial = message->serial();
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
    pending->result()->fulfill(response.payload());
  } else {
    pending->result()->reject(response.payload());
  }
  pending_messages_.erase(serial);
  pending->deref();
}

void MessageSocket::on_outgoing_response(uint64_t serial, OutgoingResponse response) {
  if (observer() != NULL)
    observer()->notify_outgoing_response(response, serial);
  ResponseMessage message(response, serial);
  Arena arena;
  Native value = arena.new_native(&message);
  send_value(value);
}

MessageSocket::PendingMessage::PendingMessage()
  : promise_(sync_promise_t<Variant, Variant>::pending()) { }

IncomingResponse MessageSocket::send_request(OutgoingRequest *request) {
  Arena arena;
  uint64_t serial = next_serial_++;
  RequestMessage message(request, serial);
  PendingMessage *pending = new (tclib::kDefaultAlloc) PendingMessage();
  pending->ref();
  pending_messages_[serial] = pending;
  Native wrapped = arena.new_native(&message);
  send_value(wrapped);
  return IncomingResponse(pending);
}

bool MessageSocket::send_value(Variant value) {
  if (!out_guard_.lock()) {
    WARN("Failed to acquire output stream");
    return false;
  }
  out_->send_value(value);
  if (!out_guard_.unlock()) {
    WARN("Failed to release output stream");
    return false;
  }
  return true;
}

OutgoingResponse::OutgoingResponse()
  : super_t(new (tclib::kDefaultAlloc) internal::OutgoingResponseData(true, Variant::null())) { }

OutgoingResponse::OutgoingResponse(Status status, Variant payload)
  : super_t(new (tclib::kDefaultAlloc) internal::OutgoingResponseData(
      status == SUCCESS,
      payload)) { }

OutgoingResponse OutgoingResponse::success(Variant value) {
  return OutgoingResponse(SUCCESS, value);
}

OutgoingResponse OutgoingResponse::failure(Variant error) {
  return OutgoingResponse(FAILURE, error);
}


internal::OutgoingResponseData::OutgoingResponseData(bool is_success,
    Variant payload)
  : is_success_(is_success)
  , payload_(payload) { }

Variant RequestData::argument(int32_t index, Variant defawlt) {
  return argument(Variant::integer(index), defawlt);
}

Variant RequestData::argument(Variant key, Variant defawlt) {
  return args_.map_get(key, defawlt);
}

Service::Service()
  : fallback_(default_fallback) {
  handler_ = new_callback(&Service::on_request, this);
}

void Service::register_method(Variant selector, Method handler) {
  methods_.set(selector, handler);
}

void Service::set_fallback(Method fallback) {
  fallback_ = fallback;
}

void Service::on_request(IncomingRequest* request, ResponseCallback response) {
  RequestData data(request->arguments(), request);
  Method *method = methods_[request->selector()];
  if (method == NULL) {
    (fallback_)(&data, response);
  } else {
    method->operator()(&data, response);
  }
}

void Service::default_fallback(RequestData *data, ResponseCallback response) {
  TextWriter writer;
  writer.write(data->selector());
  WARN("Unhandled message %s", *writer);
  response(OutgoingResponse::failure(Variant::null()));
}

StreamServiceConnector::StreamServiceConnector(InStream *in, OutStream *out)
  : insock_(in)
  , outsock_(out) { }

void StreamServiceConnector::set_default_type_registry(TypeRegistry *value) {
  insock_.set_default_type_registry(value);
}

fat_bool_t StreamServiceConnector::init(MessageSocket::RequestCallback handler) {
  F_TRY(outsock_.init());
  insock_.set_stream_factory(PushInputStream::new_instance);
  F_TRY(insock_.init());
  PushInputStream *root = static_cast<PushInputStream*>(insock_.root_stream());
  return socket_.init(root, &outsock_, handler);
}
