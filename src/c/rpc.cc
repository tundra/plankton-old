//- Copyright 2015 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "stdc.h"

#include "marshal-inl.hh"
#include "rpc.hh"

using namespace plankton;
using namespace tclib;

void Request::set_subject(Variant value) {
  subject_ = value;
}

void Request::set_selector(Variant value) {
  selector_ = value;
}

void Request::set_arguments(Variant value) {
  arguments_ = value;
}

ObjectType<Request> Request::kObjectType("rpc.Request",
    Request::new_instance,
    new_callback(&Request::init),
    new_callback(&Request::to_plankton));

Request *Request::new_instance(Variant header, Factory *factory) {
  return new (factory) Request();
}

void Request::init(Object payload, Factory *factory) {
  subject_ = payload.get_field("subject");
  selector_ = payload.get_field("selector");
  arguments_ = payload.get_field("arguments");
}

Variant Request::to_plankton(Factory *factory) {
  Object result = factory->new_object(object_type());
  result.set_field("subject", subject_);
  result.set_field("selector", selector_);
  result.set_field("arguments", arguments_);
  return result;
}

ObjectType<Response> Response::kObjectType("rpc.Response",
    Response::new_instance,
    new_callback(&Response::init),
    new_callback(&Response::to_plankton));

Response *Response::new_instance(Variant header, Factory *factory) {
  return new (factory) Response();
}

void Response::init(Object payload, Factory *factory) {
}

Variant Response::to_plankton(Factory *factory) {
  Object result = factory->new_object(object_type());
  return result;
}

MessageSocket::MessageSocket(PushInputStream *in, OutputSocket *out, RequestHandler handler)
  : in_(in)
  , out_(out)
  , handler_(handler) {
  types_.register_type(Request::object_type());
  in_->set_type_registry(&types_);
  in_->add_action(tclib::new_callback(&MessageSocket::on_incoming_message, this));
}

void MessageSocket::on_incoming_message(Variant message) {
  Request *req = message.native_as(Request::object_type());
  if (req != NULL) {
    (handler_)(req, new_callback(&MessageSocket::on_outgoing_response, this));
    return;
  }
}

void MessageSocket::on_outgoing_response(Response *response) {
  Arena arena;
  Native value = arena.new_native(response);
  out_->send_value(value);
}

void MessageSocket::send_request(Request *message) {
  Arena arena;
  Native wrapped = arena.new_native(message);
  out_->send_value(wrapped);
}
