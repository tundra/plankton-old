//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

///

#ifndef _RPC_HH
#define _RPC_HH

#include "async/promise.hh"
#include "io/file.hh"
#include "marshal.hh"
#include "plankton-inl.hh"
#include "socket.hh"

namespace plankton {

// The raw data of an rpc request.
class Request {
public:
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
};

// An incoming response is responsible for access to the result of a request.
// It provides a promise for the result and access to the resulting value or
// error. Those values are valid as long as the response is; once the response
// has been deleted any values will be disposed along with it.
class IncomingResponse {
public:
  virtual ~IncomingResponse() { }

  // Yields the promise that will resolve to the result of the request.
  virtual tclib::sync_promise_t<Variant, Variant> *operator->() = 0;

  // Basically the same as operator-> but under a different name.
  tclib::sync_promise_t<Variant, Variant> &promise() { return *operator->(); }
};

// A result constructed by the handler of a request and returned to the rpc
// framework as the result of an operation.
class OutgoingResponse {
public:
  enum Status {
    SUCCESS,
    FAILURE
  };

  // Create an empty response, successful with a null payload.
  OutgoingResponse();

  // Create a response of the given type with the given payload.
  OutgoingResponse(Status status, Variant payload);

  // Is this a successful response?
  bool is_success() { return status_ == SUCCESS; }

  // The value or error, depending on whether this is a successful response.
  Variant payload() { return payload_; }

private:
  Status status_;
  Variant payload_;
};

class RequestMessage;
class ResponseMessage;

// A socket you can send and receive requests through.
class MessageSocket {
public:
  typedef tclib::callback_t<void(OutgoingResponse*)> ResponseHandler;
  typedef tclib::callback_t<void(Request*, ResponseHandler)> RequestHandler;

  // Initializes a socket that receives incoming requests and responses through
  // the given input stream and sends its own requests and responses through the
  // output stream.
  //
  // The callback will be called on incoming requests; the first argument will
  // be the actual request, the second a callback to call with the response. The
  // response value is only valid until the callback returns.
  MessageSocket(PushInputStream *in, OutputSocket *out, RequestHandler handler);

  // Writes a request to the outgoing socket and returns a promise for a
  // response received on the incoming socket.
  IncomingResponse *send_request(Request *request);

private:
  class PendingMessage;
  typedef platform_hash_map<uint64_t, PendingMessage*> PendingMessageMap;
  void on_incoming_message(Variant message);
  void on_incoming_request(RequestMessage *request);
  void on_incoming_response(ResponseMessage *response);
  void on_outgoing_response(uint64_t serial, OutgoingResponse* message);
  PushInputStream *in_;
  OutputSocket *out_;
  RequestHandler handler_;
  TypeRegistry types_;
  uint64_t next_serial_;
  PendingMessageMap pending_messages_;
};

} // namespace plankton

#endif // _RPC_HH
