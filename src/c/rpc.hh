//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

///

#ifndef _RPC_HH
#define _RPC_HH

#include "io/file.hh"
#include "marshal.hh"
#include "plankton-inl.hh"
#include "socket.hh"

namespace plankton {

// An individual plankton rpc request.
class Request {
public:
  static ObjectType<Request> *object_type() { return &kObjectType; }

  void set_subject(Variant value);
  void set_selector(Variant value);
  void set_arguments(Variant value);
public:
  Variant to_plankton(Factory *factory);
  static Request *new_instance(Variant header, Factory *factory);
  void init(Object payload, Factory *factory);
  static ObjectType<Request> kObjectType;

  Variant subject_;
  Variant selector_;
  Variant arguments_;
};

class Response {
public:
  static ObjectType<Response> *object_type() { return &kObjectType; }
private:
  Variant to_plankton(Factory *factory);
  static Response *new_instance(Variant header, Factory *factory);
  void init(Object payload, Factory *factory);
  static ObjectType<Response> kObjectType;

};

// A socket you can send messages to and read responses from.
class MessageSocket {
public:
  typedef tclib::callback_t<void(Response*)> ResponseHandler;
  typedef tclib::callback_t<void(Request*, ResponseHandler)> RequestHandler;
  MessageSocket(PushInputStream *in, OutputSocket *out, RequestHandler handler);
  void send_request(Request *request);
private:
  void on_incoming_message(Variant message);
  void on_outgoing_response(Response* message);
  PushInputStream *in_;
  OutputSocket *out_;
  RequestHandler handler_;
  TypeRegistry types_;
};

} // namespace plankton

#endif // _RPC_HH
