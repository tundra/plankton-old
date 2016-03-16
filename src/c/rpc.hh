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
#include "utils/fatbool.hh"
#include "utils/refcount.hh"

// Basic rpc mechanism. This lies above the raw socket support but below the
// high-level service abstraction.
//
// The terminology is as follows: data you receive over the wire, whether it's
// requests from others or responses to your own requests, are called incoming.
// Requests you send out and responses to others' requests are called outgoing.
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

protected:
  virtual size_t instance_size() { return sizeof(*this); }

private:
  Arena arena_;
  bool is_success_;
  Variant payload_;
};

class RequestMessage;
class ResponseMessage;

} // namespace internal

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

  void set_arguments(size_t argc, Variant *argv);

  Factory *factory() { return &arena_; }

private:
  Variant subject_;
  Variant selector_;
  Variant arguments_;
  Arena arena_;
};

// An incoming request. It's basically a read-only view of an outgoing request.
class IncomingRequest {
public:
  IncomingRequest(OutgoingRequest *outgoing) : outgoing_(outgoing) { }
  Variant subject() { return outgoing_->subject(); }
  Variant selector() { return outgoing_->selector(); }
  Variant arguments() { return outgoing_->arguments(); }

  // Returns a factory that can be used for constructing values that can be
  // returned as the result of this request.
  Factory *factory() { return outgoing_->factory(); }

private:
  OutgoingRequest *outgoing_;
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
  IncomingResponse() : super_t() { }

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

class MessageSocket;

// A socket observer is a utility that can be attached to a socket and will
// be notified of messages coming in and out. It's convenient for tracing and
// debugging.
class MessageSocketObserver {
public:
  MessageSocketObserver();
  virtual ~MessageSocketObserver();

  // Called whenever the service this observer is attached to receives a
  // request, before that request has been passed to the implementation of the
  // service.
  virtual void on_incoming_request(IncomingRequest *request, uint64_t serial) = 0;

  // Called whenever a request has been processed and a response to return
  // to the caller has been received by the service framework but before it has
  // been delivered to the caller.
  virtual void on_outgoing_response(OutgoingResponse response, uint64_t serial) = 0;

  // Installs this observer on the given socket. An observer can only be
  // installed on one socket at a time and if multiple observers are installed
  // they must be uninstalled in the reverse order they were installed.
  void install(MessageSocket *subject);

  // Uninstalls this observer from the given socket. If multiple observers have
  // been installed they must be uninstalled in the reverse order they were
  // installed.
  void uninstall();

private:
  friend class MessageSocket;

  // Called by the message socket on requests.
  void notify_incoming_request(IncomingRequest *request, uint64_t serial);

  // Called by the message socket on responses.
  void notify_outgoing_response(OutgoingResponse response, uint64_t serial);

  MessageSocket *subject_;
  MessageSocket *subject() { return subject_; }
  MessageSocketObserver *next_;
  MessageSocketObserver *next() { return next_; }
  bool is_installed_;
};

// A socket you can send and receive requests through.
class MessageSocket {
public:
  // A callback that can be used to deliver a response to a request.
  typedef tclib::callback_t<void(OutgoingResponse)> ResponseCallback;

  class SerialHasher;

  // Wrapper around a message serial number. This is to make it explicit how the
  // hash map should deal with serials rather than rely on built-in handling of
  // raw uint64_ts which is inconsistent between platforms.
  class Serial {
  public:
    Serial(uint64_t value) : value_(value) { }
    bool operator==(const Serial &other) const { return value_ == other.value_; }

  private:
    friend class SerialHasher;
    uint64_t value_;
  };

  // Controls how serials are hashed, again to avoid platform-dependent
  // confusion about the default way to handle uint64_ts.
  class SerialHasher {
  public:
    size_t operator()(const Serial &key) const;
    // MSVC hash map stuff.
    static const size_t bucket_size = 4;
    bool operator()(const Serial &a, const Serial &b);
  };

  // The type of callback that will be invoked to handle incoming requests. The
  // response callback can be used to return a response asynchronously. The
  // response callback is thread safe, so it is safe both to pass it between
  // threads and to invoke it from a different thread than the one that
  // originated it.
  typedef tclib::callback_t<void(IncomingRequest*, ResponseCallback)> RequestCallback;

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
  MessageSocket(PushInputStream *in, OutputSocket *out, RequestCallback handler);

  // Initialize an empty socket.
  fat_bool_t init(PushInputStream *in, OutputSocket *out, RequestCallback handler);

  // Writes a request to the outgoing socket and returns a promise for a
  // response received on the incoming socket.
  IncomingResponse send_request(OutgoingRequest *request);

private:
  class PendingMessage;
  typedef platform_hash_map<Serial, PendingMessage*, SerialHasher> PendingMessageMap;
  void on_incoming_message(ParsedMessage *message);
  void on_incoming_request(internal::RequestMessage *request);
  void on_incoming_response(VariantOwner *owner, internal::ResponseMessage *response);
  void on_outgoing_response(uint64_t serial, OutgoingResponse message);

  // Write a value on the output stream. This method is thread safe whereas
  // just writing directly isn't.
  bool send_value(Variant value);

  PushInputStream *in_;
  OutputSocket *out_;
  tclib::NativeMutex out_guard_;
  RequestCallback handler_;
  TypeRegistry types_;
  uint64_t next_serial_;
  PendingMessageMap pending_messages_;

  friend class MessageSocketObserver;
  MessageSocketObserver *observer() { return observer_; }
  MessageSocketObserver *observer_;
};

// Utility for fetching individual request arguments.
class RequestData {
public:
  // Returns the index'th positional argument to a request.
  Variant argument(int32_t index, Variant defawlt = Variant::null());

  // Returns a factory that can be used to allocate the result of a request
  // callback. The factory is only guaranteed to stick around for the duration
  // of the callback so you can return values allocated using it but not keep
  // them for later.
  Factory *factory() { return request_->factory(); }

  // Returns this request's selector.
  Variant selector() { return request_->selector(); }

private:
  friend class Service;
  RequestData(Variant args, IncomingRequest *request) : args_(args), request_(request) { }
  Variant args_;
  IncomingRequest *request_;
};

class Service {
public:
  typedef tclib::callback_t<void(OutgoingResponse)> ResponseCallback;
  typedef tclib::callback_t<void(RequestData*, ResponseCallback)> Method;

  Service();
  virtual ~Service() { }

  // Adds a method to the set understood by this service.
  void register_method(Variant selector, Method handler);

  // Sets the fallback method to call for requests with selectors with no
  // registered handler. The default behavior is to log a warning and fail with
  // the null value.
  void set_fallback(Method fallback);

  // Returns the callback to pass to a message socket that will dispatch
  // messages to this service.
  MessageSocket::RequestCallback handler() { return handler_; }

private:
  // The fallback to use if none have been set explicitly.
  static void default_fallback(RequestData *data, ResponseCallback callback);

  // General handler for incoming requests.
  void on_request(IncomingRequest *request, ResponseCallback response);

  Arena arena_;
  VariantMap<Method> methods_;
  Method fallback_;
  MessageSocket::RequestCallback handler_;
};

// Utility that connects an in and an out stream as one end of a plankton rpc
// connection. This doesn't really add much, just wires together other types.
class StreamServiceConnector : public tclib::DefaultDestructable {
public:
  StreamServiceConnector(tclib::InStream *in, tclib::OutStream *out);
  virtual ~StreamServiceConnector() { }
  virtual void default_destroy() { tclib::default_delete_concrete(this); }

  // Initializes the components of this connector, setting the given handler
  // up as the one to handle incoming requests.
  fat_bool_t init(MessageSocket::RequestCallback handler);

  // The underlying input socket.
  InputSocket *input() { return &insock_; }

  OutputSocket *output() { return &outsock_; }

  // The underlying message socket.
  MessageSocket *socket() { return &socket_; }

  void set_default_type_registry(TypeRegistry *value);

  // Keep running and processing messages as long as they come in on the input
  // stream.
  fat_bool_t process_all_messages() { return insock_.process_all_instructions(); }

private:
  InputSocket insock_;
  OutputSocket outsock_;
  MessageSocket socket_;
};

} // namespace rpc
} // namespace plankton

#endif // _RPC_HH
