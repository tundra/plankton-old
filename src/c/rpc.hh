//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

///

#ifndef _RPC_HH
#define _RPC_HH

#include "async/promise.hh"
#include "io/file.hh"
#include "marshal.hh"
#include "plankton-inl.hh"
#include "refcount.hh"
#include "socket.hh"

// Basic rpc mechanism. This lies above the raw socket support but below the
// high-level service abstraction.
//
// The terminology is as follow: data you receive over the wire, whether it's
// requests from others or responses to your own requests, are called incoming.
// Requests you send out and responses to your requests are called incoming.
// So you receive incoming requests and get back incoming responses, and you
// construct and transmit outgoing requests and outgoing responses. These all
// behave differently so they're represented by different but similar looking
// types.

namespace plankton {
namespace rpc {

namespace internal {

// Data backing an incoming response. Don't use this directly.
class IncomingResponseData : public tclib::refcount_shared_t {
public:
  virtual ~IncomingResponseData() { }
  virtual tclib::sync_promise_t<Variant, Variant> *result() = 0;
};

// Data backing an outgoing response. Don't use this directly.
class OutgoingResponseData : public tclib::refcount_shared_t {
public:
  OutgoingResponseData(bool is_success, Variant payload);
  ~OutgoingResponseData() { }
  bool is_success() { return is_success_; }
  Variant payload() { return payload_; }
  Factory *factory() { return &arena_; }
private:
  Arena arena_;
  bool is_success_;
  Variant payload_;
};

class RequestMessage;
class ResponseMessage;

}

// The raw data of an rpc request.
class OutgoingRequest {
public:
  OutgoingRequest(Variant subject = Variant::null(),
      Variant selector = Variant::null(), size_t argc = 0, Variant *argv = NULL);

  // The subject, the receiver of the request.
  void set_subject(Variant value) { subject_ = value; }
  Variant subject() { return subject_; }

  // The selector, the name of the operation to be performed on the subject.
  void set_selector(Variant value) { selector_ = value; }
  Variant selector() { return selector_; }

  // The arguments, typically a dictionary of values to pass to the operation.
  void set_arguments(Variant value) { arguments_ = value; }
  Variant arguments() { return arguments_; }

private:
  Variant subject_;
  Variant selector_;
  Variant arguments_;
  Arena arena_;
};

// An incoming response is responsible for access to the result of a request.
// It provides a promise for the result and access to the resulting value or
// error.
//
// Incoming responses are backed by ref counted data so you can pass them around
// by value and the data will be disposed when there are no more references
// left.
class IncomingResponse
    : public tclib::refcount_reference_t<internal::IncomingResponseData> {
public:
  typedef tclib::refcount_reference_t<internal::IncomingResponseData> super_t;

  // Yields the promise that will resolve to the result of the request.
  tclib::sync_promise_t<Variant, Variant> *operator->() { return data()->result(); }

  // Basically the same as operator-> but under a different name.
  tclib::sync_promise_t<Variant, Variant> &promise() { return *data()->result(); }

private:
  friend class MessageSocket;
  IncomingResponse(internal::IncomingResponseData *data) : super_t(data) { }
  internal::IncomingResponseData *data() { return refcount_shared(); }
};

// A result constructed by the handler of a request and returned to the rpc
// framework as the result of an operation.
class OutgoingResponse
    : public tclib::refcount_reference_t<internal::OutgoingResponseData> {
public:
  typedef tclib::refcount_reference_t<internal::OutgoingResponseData> super_t;

  // Response status codes.
  enum Status {
    SUCCESS,
    FAILURE
  };

  // Create an empty response, successful with a null payload.
  OutgoingResponse();

  // Create a response of the given type with the given payload.
  OutgoingResponse(Status status, Variant payload);

  // Is this a successful response?
  bool is_success() { return data()->is_success(); }

  // The value or error, depending on whether this is a successful response.
  Variant payload() { return data()->payload(); }

  // Returns a factory that can be used to allocate the response, or take
  // ownership if it has been allocated elsewhere.
  Factory *factory() { return data()->factory(); }

  // Returns a successful response with the given value.
  static OutgoingResponse success(Variant value);

  // Returns a failure response with the given error.
  static OutgoingResponse failure(Variant error);

private:
  internal::OutgoingResponseData *data() { return refcount_shared(); }

};

// A socket you can send and receive requests through.
class MessageSocket {
public:
  typedef tclib::callback_t<void(OutgoingResponse)> ResponseHandler;
  typedef tclib::callback_t<void(OutgoingRequest*, ResponseHandler)> RequestHandler;

  // Initializes an empty socket. If this constructor is used you must then
  // use init() to initialize the socket. Alternatively use the constructor
  // below to both create an initialize the socket in one go.
  MessageSocket();

  // Initializes a socket that receives incoming requests and responses through
  // the given input stream and sends its own requests and responses through the
  // output stream.
  //
  // The callback will be called on incoming requests; the first argument will
  // be the actual request, the second a callback to call with the response. The
  // response value is only valid until the callback returns.
  MessageSocket(PushInputStream *in, OutputSocket *out, RequestHandler handler);

  // Initialize an empty socket.
  void init(PushInputStream *in, OutputSocket *out, RequestHandler handler);

  // Writes a request to the outgoing socket and returns a promise for a
  // response received on the incoming socket.
  IncomingResponse send_request(OutgoingRequest *request);

private:
  class PendingMessage;
  typedef platform_hash_map<uint64_t, PendingMessage*> PendingMessageMap;
  void on_incoming_message(ParsedMessage *message);
  void on_incoming_request(internal::RequestMessage *request);
  void on_incoming_response(VariantOwner *owner, internal::ResponseMessage *response);
  void on_outgoing_response(uint64_t serial, OutgoingResponse message);
  PushInputStream *in_;
  OutputSocket *out_;
  RequestHandler handler_;
  TypeRegistry types_;
  uint64_t next_serial_;
  PendingMessageMap pending_messages_;
};

class Service {
public:
  typedef tclib::callback_t<void(OutgoingResponse)> ResponseCallback;
  typedef tclib::callback_t<void(Variant, ResponseCallback)> GenericMethod;
  typedef tclib::callback_t<void(ResponseCallback)> MethodZero;
  typedef tclib::callback_t<void(Variant, ResponseCallback)> MethodOne;

  Service();

  // Adds a method to the set understood by this service.
  void register_method(Variant selector, MethodZero handler);

  // Adds a method to the set understood by this service.
  void register_method(Variant selector, MethodOne handler);

  // Returns the callback to pass to a message socket that will dispatch
  // messages to this service.
  MessageSocket::RequestHandler handler() { return handler_; }

private:
  // General handler for incoming requests.
  void on_request(OutgoingRequest* request, ResponseCallback response);

  static void method_zero_trampoline(MethodZero delegate, Variant args,
      ResponseCallback callback);
  static void method_one_trampoline(MethodOne delegate, Variant args,
      ResponseCallback callback);

  Arena arena_;
  VariantMap<GenericMethod> methods_;
  MessageSocket::RequestHandler handler_;
};

} // namespace rpc
} // namespace plankton

#endif // _RPC_HH
