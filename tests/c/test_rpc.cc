//- Copyright 2015 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "async/promise-inl.hh"
#include "io/file.hh"
#include "marshal-inl.hh"
#include "rpc.hh"
#include "sync/mutex.hh"
#include "sync/semaphore.hh"
#include "sync/thread.hh"
#include "test/asserts.hh"
#include "test/unittest.hh"

using namespace plankton;
using namespace plankton::rpc;
using namespace tclib;

// A bounded concurrent io stream that allows any number of concurrent readers
// and writers. It doesn't necessarily scale super well but it is simple and
// the concurrency control is (famous last words) solid.
class ByteBufferStream : public tclib::IoStream {
public:
  ByteBufferStream(size_t capacity);
  ~ByteBufferStream();
  virtual size_t read_bytes(void *dest, size_t size);
  virtual size_t write_bytes(void *src, size_t size);
  virtual bool at_eof();
  virtual size_t vprintf(const char *fmt, va_list argp);
  virtual bool flush();

private:
  size_t capacity_;
  size_t next_read_cursor_;
  size_t next_write_cursor_;
  tclib::NativeSemaphore readable_;
  tclib::NativeSemaphore writable_;
  tclib::NativeMutex buffer_mutex_;
  byte_t *buffer_;
};

ByteBufferStream::ByteBufferStream(size_t capacity)
  : capacity_(capacity)
  , next_read_cursor_(0)
  , next_write_cursor_(0)
  , readable_(0)
  , writable_(capacity)
  , buffer_(new byte_t[capacity]) {
  ASSERT_TRUE(readable_.initialize());
  ASSERT_TRUE(writable_.initialize());
  ASSERT_TRUE(buffer_mutex_.initialize());
}

ByteBufferStream::~ByteBufferStream() {
  delete[] buffer_;
}

size_t ByteBufferStream::read_bytes(void *raw_dest, size_t size) {
  byte_t *dest = static_cast<byte_t*>(raw_dest);
  for (size_t i = 0; i < size; i++) {
    readable_.acquire();
    buffer_mutex_.lock();
    dest[i] = buffer_[next_read_cursor_];
    next_read_cursor_ = (next_read_cursor_ + 1) % capacity_;
    buffer_mutex_.unlock();
    writable_.release();
  }
  return size;
}

size_t ByteBufferStream::write_bytes(void *raw_src, size_t size) {
  byte_t *src = static_cast<byte_t*>(raw_src);
  for (size_t i = 0; i < size; i++) {
    writable_.acquire();
    buffer_mutex_.lock();
    buffer_[next_write_cursor_] = src[i];
    next_write_cursor_ = (next_write_cursor_ + 1) % capacity_;
    buffer_mutex_.unlock();
    readable_.release();
  }
  return size;
}

bool ByteBufferStream::at_eof() {
  return false;
}

size_t ByteBufferStream::vprintf(const char *fmt, va_list argp) {
  return 0;
}

bool ByteBufferStream::flush() {
  return true;
}

TEST(rpc, byte_buffer_simple) {
  ByteBufferStream stream(374);
  for (size_t io = 0; io < 374; io++) {
    size_t offset = io * 7;
    for (size_t ii = 0; ii < 373; ii++) {
      byte_t value = static_cast<byte_t>(offset + (5 * ii));
      ASSERT_EQ(1, stream.write_bytes(&value, 1));
    }
    for (size_t ii = 0; ii < 373; ii++) {
      byte_t value = 0;
      ASSERT_EQ(1, stream.read_bytes(&value, 1));
      ASSERT_EQ(value, static_cast<byte_t>(offset + (5 * ii)));
    }
  }
}

class Slice {
public:
  Slice(ByteBufferStream *nexus, NativeSemaphore *lets_go, Slice **slices, size_t index);
  void start();
  void join();
  static const size_t kSliceCount = 16;
  static const size_t kStepCount = 1600;
private:
  void *run_producer();
  void *run_distributer();
  void *run_validator();
  byte_t get_value(size_t step) { return (index_ << 4) + (step & 0xF); }
  size_t get_origin(byte_t value) { return value >> 4; }
  size_t get_step(byte_t value) { return value & 0xF; }
  ByteBufferStream *nexus_;
  NativeSemaphore *lets_go_;
  ByteBufferStream stream_;
  Slice **slices_;
  size_t index_;
  NativeThread producer_;
  NativeThread distributer_;
  NativeThread validator_;
};

Slice::Slice(ByteBufferStream *nexus, NativeSemaphore *lets_go, Slice **slices, size_t index)
  : nexus_(nexus)
  , lets_go_(lets_go)
  , stream_(57 + index)
  , slices_(slices)
  , index_(index)
  , producer_(new_callback(&Slice::run_producer, this))
  , distributer_(new_callback(&Slice::run_distributer, this))
  , validator_(new_callback(&Slice::run_validator, this)) { }

void Slice::start() {
  validator_.start();
  distributer_.start();
  producer_.start();
}

void Slice::join() {
  validator_.join();
  distributer_.join();
  producer_.join();
}

void *Slice::run_producer() {
  lets_go_->acquire();
  for (size_t i = 0; i < kStepCount; i++) {
    byte_t value = get_value(i);
    nexus_->write_bytes(&value, 1);
  }
  return NULL;
}

void *Slice::run_distributer() {
  for (size_t i = 0; i < kStepCount; i++) {
    byte_t value = 0;
    nexus_->read_bytes(&value, 1);
    size_t origin = get_origin(value);
    slices_[origin]->stream_.write_bytes(&value, 1);
  }
  return NULL;
}

void *Slice::run_validator() {
  size_t counts[kSliceCount];
  for (size_t i = 0; i < kSliceCount; i++)
    counts[i] = 0;
  for (size_t i = 0; i < kStepCount; i++) {
    byte_t value = 0;
    stream_.read_bytes(&value, 1);
    size_t origin = get_origin(value);
    ASSERT_EQ(index_, origin);
    size_t step = get_step(value);
    counts[step]++;
  }
  for (size_t i = 0; i < kSliceCount; i++)
    ASSERT_EQ(kStepCount / kSliceCount, counts[i]);
  return NULL;
}

TEST(rpc, byte_buffer_concurrent) {
  // This is a bit intricate. It works like this. There's N producers all
  // writing concurrently to the same stream, the nexus. Then there's N threads,
  // the distributers, reading values back out from the nexus. Each value is
  // tagged with which produces wrote it, the distributer writes values from
  // producer i to stream i. Each of these N streams have a thread reading
  // values out and checking that they all came from producer i and that the
  // payload is as expected. That's it. A lot going on.
  ByteBufferStream nexus(41);
  Slice *slices[Slice::kSliceCount];
  NativeSemaphore lets_go(0);
  ASSERT_TRUE(lets_go.initialize());
  for (size_t i = 0; i < Slice::kSliceCount; i++)
    slices[i] = new Slice(&nexus, &lets_go, slices, i);
  for (size_t i = 0; i < Slice::kSliceCount; i++)
    slices[i]->start();
  for (size_t i = 0; i < Slice::kSliceCount; i++)
    lets_go.release();
  for (size_t i = 0; i < Slice::kSliceCount; i++)
    slices[i]->join();
  for (size_t i = 0; i < Slice::kSliceCount; i++)
    delete slices[i];
}

static void handle_request(MessageSocket::ResponseHandler *handler_out,
    OutgoingRequest *request, MessageSocket::ResponseHandler handler) {
  ASSERT_TRUE(request->subject() == Variant("test_subject"));
  ASSERT_TRUE(request->selector() == Variant("test_selector"));
  ASSERT_TRUE(request->arguments() == Variant("test_arguments"));
  *handler_out = handler;
}

class RpcChannel {
public:
  RpcChannel(MessageSocket::RequestHandler handler);
  bool process_next_instruction();
  MessageSocket *operator->() { return &sock_; }
private:
  ByteBufferStream bytes_;
  OutputSocket outsock_;
  InputSocket insock_;
  MessageSocket sock_;
};

RpcChannel::RpcChannel(MessageSocket::RequestHandler handler)
  : bytes_(1024)
  , outsock_(&bytes_)
  , insock_(&bytes_) {
  outsock_.init();
  insock_.set_stream_factory(PushInputStream::new_instance);
  ASSERT_TRUE(insock_.init());
  PushInputStream *root = static_cast<PushInputStream*>(insock_.root_stream());
  sock_.init(root, &outsock_, handler);
}

bool RpcChannel::process_next_instruction() {
  return insock_.process_next_instruction();
}

TEST(rpc, roundtrip) {
  MessageSocket::ResponseHandler on_response;
  RpcChannel channel(new_callback(handle_request, &on_response));
  OutgoingRequest request("test_subject", "test_selector");
  request.set_arguments("test_arguments");
  IncomingResponse incoming = channel->send_request(&request);
  ASSERT_FALSE(incoming->is_resolved());
  while (on_response.is_empty())
    ASSERT_TRUE(channel.process_next_instruction());
  ASSERT_FALSE(incoming->is_resolved());
  on_response(OutgoingResponse::success(Variant::integer(18)));
  while (!incoming->is_resolved())
    ASSERT_TRUE(channel.process_next_instruction());
  ASSERT_TRUE(Variant::integer(18) == incoming->peek_value(Variant::null()));
}

class EchoService : public plankton::rpc::Service {
public:
  void echo(Variant value, ResponseCallback response);
  void ping(ResponseCallback response);
  EchoService();
};

EchoService::EchoService() {
  register_method("echo", tclib::new_callback(&EchoService::echo, this));
  register_method("ping", tclib::new_callback(&EchoService::ping, this));
}

void EchoService::echo(Variant value, ResponseCallback callback) {
  callback(OutgoingResponse::success(value));
}

void EchoService::ping(ResponseCallback callback) {
  callback(OutgoingResponse::success("pong"));
}

TEST(rpc, service) {
  EchoService echo;
  RpcChannel channel(echo.handler());
  Variant args[1] = {43};
  OutgoingRequest req0(Variant::null(), "echo", 1, args);
  IncomingResponse inc0 = channel->send_request(&req0);
  OutgoingRequest req1(Variant::null(), "echo");
  IncomingResponse inc1 = channel->send_request(&req1);
  OutgoingRequest req2(Variant::null(), "ping");
  IncomingResponse inc2 = channel->send_request(&req2);
  while (!inc2->is_resolved())
    channel.process_next_instruction();
  ASSERT_EQ(43, inc0->peek_value(Variant::null()).integer_value());
  ASSERT_TRUE(inc1->peek_value(10).is_null());
  ASSERT_TRUE(Variant::string("pong") == inc2->peek_value(10));
}
