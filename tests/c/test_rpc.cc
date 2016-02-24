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
class ByteBufferStream : public tclib::InStream, public tclib::OutStream {
public:
  class Entry {
  public:
    Entry() : is_eof(false), value(0) { }
    Entry(bool _is_eof, byte_t _value) : is_eof(_is_eof), value(_value) { }
    bool is_eof;
    byte_t value;
  };
  ByteBufferStream(uint32_t capacity);
  ~ByteBufferStream();
  virtual void default_destroy() { default_delete_concrete(this); }
  virtual bool read_sync(read_iop_state_t *op);
  virtual bool write_sync(write_iop_state_t *op);
  virtual bool flush();
  virtual bool close();
  void write_entry(Entry entry);

private:
  size_t capacity_;
  size_t next_read_cursor_;
  size_t next_write_cursor_;
  tclib::NativeSemaphore readable_;
  tclib::NativeSemaphore writable_;
  tclib::NativeMutex buffer_mutex_;
  Entry *buffer_;
};

ByteBufferStream::ByteBufferStream(uint32_t capacity)
  : capacity_(capacity)
  , next_read_cursor_(0)
  , next_write_cursor_(0)
  , readable_(0)
  , writable_(capacity)
  , buffer_(new Entry[capacity]) {
  ASSERT_TRUE(readable_.initialize());
  ASSERT_TRUE(writable_.initialize());
  ASSERT_TRUE(buffer_mutex_.initialize());
}

ByteBufferStream::~ByteBufferStream() {
  delete[] buffer_;
}

bool ByteBufferStream::read_sync(read_iop_state_t *op) {
  byte_t *dest = static_cast<byte_t*>(op->dest_);
  size_t size = op->dest_size_;
  size_t offset = 0;
  bool at_eof = false;
  for (; offset < size; offset++) {
    readable_.acquire();
    buffer_mutex_.lock();
    Entry entry = buffer_[next_read_cursor_];
    if (entry.is_eof) {
      readable_.release();
      buffer_mutex_.unlock();
      at_eof = true;
      break;
    } else {
      dest[offset] = entry.value;
      next_read_cursor_ = (next_read_cursor_ + 1) % capacity_;
      buffer_mutex_.unlock();
      writable_.release();
    }
  }
  read_iop_state_deliver(op, offset, at_eof);
  return true;
}

bool ByteBufferStream::write_sync(write_iop_state_t *op) {
  const byte_t *src = static_cast<const byte_t*>(op->src);
  for (size_t i = 0; i < op->src_size; i++)
    write_entry(Entry(false, src[i]));
  write_iop_state_deliver(op, op->src_size);
  return true;
}

void ByteBufferStream::write_entry(Entry entry) {
  writable_.acquire();
  buffer_mutex_.lock();
  buffer_[next_write_cursor_] = entry;
  next_write_cursor_ = (next_write_cursor_ + 1) % capacity_;
  buffer_mutex_.unlock();
  readable_.release();
}

bool ByteBufferStream::flush() {
  return true;
}

bool ByteBufferStream::close() {
  write_entry(Entry(true, 0));
  return true;
}

TEST(rpc, byte_buffer_simple) {
  ByteBufferStream stream(374);
  for (size_t io = 0; io < 374; io++) {
    size_t offset = io * 7;
    for (size_t ii = 0; ii < 373; ii++) {
      byte_t value = static_cast<byte_t>(offset + (5 * ii));
      WriteIop iop(&stream, &value, 1);
      ASSERT_TRUE(iop.execute());
      ASSERT_EQ(1, iop.bytes_written());
    }
    for (size_t ii = 0; ii < 373; ii++) {
      byte_t value = 0;
      ReadIop iop(&stream, &value, 1);
      ASSERT_TRUE(iop.execute());
      ASSERT_EQ(1, iop.bytes_read());
      ASSERT_EQ(value, static_cast<byte_t>(offset + (5 * ii)));
    }
  }
  ASSERT_TRUE(stream.close());
  byte_t value = 0;
  ReadIop iop(&stream, &value, 1);
  ASSERT_TRUE(iop.execute());
  ASSERT_EQ(0, iop.bytes_read());
  ASSERT_TRUE(iop.at_eof());
}

TEST(rpc, byte_buffer_delayed_eof) {
  // Check that if we close the stream before the contents have all been read
  // those contents are still available to be read before the stream eofs.
  ByteBufferStream stream(374);
  byte_t buf[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  WriteIop write(&stream, buf, 10);
  ASSERT_TRUE(write.execute());
  ASSERT_EQ(10, write.bytes_written());
  ASSERT_TRUE(stream.close());
  memset(buf, 0, 10);
  ReadIop read(&stream, buf, 10);
  ASSERT_TRUE(read.execute());
  ASSERT_EQ(10, read.bytes_read());
  ASSERT_FALSE(read.at_eof());
  read.recycle();
  ASSERT_TRUE(read.execute());
  ASSERT_EQ(0, read.bytes_read());
  ASSERT_TRUE(read.at_eof());
  read.recycle();
  ASSERT_TRUE(read.execute());
  ASSERT_EQ(0, read.bytes_read());
  ASSERT_TRUE(read.at_eof());
}

class Slice {
public:
  Slice(ByteBufferStream *nexus, NativeSemaphore *lets_go, Slice **slices, uint32_t index);
  void start();
  void join();
  static const uint32_t kSliceCount = 16;
  static const uint32_t kStepCount = 1600;
private:
  opaque_t run_producer();
  opaque_t run_distributer();
  opaque_t run_validator();
  byte_t get_value(size_t step) { return static_cast<byte_t>((index_ << 4) + (step & 0xF)); }
  size_t get_origin(byte_t value) { return value >> 4; }
  size_t get_step(byte_t value) { return value & 0xF; }
  ByteBufferStream *nexus_;
  NativeSemaphore *lets_go_;
  ByteBufferStream stream_;
  Slice **slices_;
  uint32_t index_;
  NativeThread producer_;
  NativeThread distributer_;
  NativeThread validator_;
};

Slice::Slice(ByteBufferStream *nexus, NativeSemaphore *lets_go, Slice **slices, uint32_t index)
  : nexus_(nexus)
  , lets_go_(lets_go)
  , stream_(57 + index)
  , slices_(slices)
  , index_(index) {
  producer_ = new_callback(&Slice::run_producer, this);
  distributer_ = new_callback(&Slice::run_distributer, this);
  validator_ = new_callback(&Slice::run_validator, this);
}

void Slice::start() {
  validator_.start();
  distributer_.start();
  producer_.start();
}

void Slice::join() {
  ASSERT_TRUE(validator_.join(NULL));
  ASSERT_TRUE(distributer_.join(NULL));
  ASSERT_TRUE(producer_.join(NULL));
}

opaque_t Slice::run_producer() {
  lets_go_->acquire();
  for (size_t i = 0; i < kStepCount; i++) {
    byte_t value = get_value(i);
    WriteIop iop(nexus_, &value, 1);
    ASSERT_TRUE(iop.execute());
  }
  return o0();
}

opaque_t Slice::run_distributer() {
  for (size_t i = 0; i < kStepCount; i++) {
    byte_t value = 0;
    ReadIop read_iop(nexus_, &value, 1);
    ASSERT_TRUE(read_iop.execute());
    size_t origin = get_origin(value);
    WriteIop write_iop(&slices_[origin]->stream_, &value, 1);
    ASSERT_TRUE(write_iop.execute());
  }
  return o0();
}

opaque_t Slice::run_validator() {
  size_t counts[kSliceCount];
  for (size_t i = 0; i < kSliceCount; i++)
    counts[i] = 0;
  for (size_t i = 0; i < kStepCount; i++) {
    byte_t value = 0;
    ReadIop iop(&stream_, &value, 1);
    iop.execute();
    size_t origin = get_origin(value);
    ASSERT_EQ(index_, origin);
    size_t step = get_step(value);
    counts[step]++;
  }
  for (size_t i = 0; i < kSliceCount; i++)
    ASSERT_EQ(kStepCount / kSliceCount, counts[i]);
  return o0();
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
  for (uint32_t i = 0; i < Slice::kSliceCount; i++)
    slices[i] = new Slice(&nexus, &lets_go, slices, i);
  for (uint32_t i = 0; i < Slice::kSliceCount; i++)
    slices[i]->start();
  for (uint32_t i = 0; i < Slice::kSliceCount; i++)
    lets_go.release();
  for (uint32_t i = 0; i < Slice::kSliceCount; i++)
    slices[i]->join();
  for (uint32_t i = 0; i < Slice::kSliceCount; i++)
    delete slices[i];
}

static void handle_request(MessageSocket::ResponseCallback *callback_out,
    IncomingRequest *request, MessageSocket::ResponseCallback callback) {
  ASSERT_TRUE(request->subject() == Variant("test_subject"));
  ASSERT_TRUE(request->selector() == Variant("test_selector"));
  ASSERT_TRUE(request->arguments() == Variant("test_arguments"));
  *callback_out = callback;
}

// An rpc channel that uses the same buffer for requests and responses.
class SharedRpcChannel {
public:
  SharedRpcChannel(MessageSocket::RequestCallback handler);
  bool process_next_instruction();
  MessageSocket *operator->() { return channel()->socket(); }
  StreamServiceConnector *channel() { return &channel_; }
  bool close() { return bytes_.close(); }
private:
  ByteBufferStream bytes_;
  StreamServiceConnector channel_;
};

SharedRpcChannel::SharedRpcChannel(MessageSocket::RequestCallback handler)
  : bytes_(1024)
  , channel_(&bytes_, &bytes_) {
  ASSERT_TRUE(channel_.init(handler));
}

bool SharedRpcChannel::process_next_instruction() {
  return channel()->input()->process_next_instruction(NULL);
}

TEST(rpc, roundtrip) {
  MessageSocket::ResponseCallback on_response;
  SharedRpcChannel channel(new_callback(handle_request, &on_response));
  OutgoingRequest request("test_subject", "test_selector");
  request.set_arguments("test_arguments");
  IncomingResponse incoming = channel->send_request(&request);
  ASSERT_FALSE(incoming->is_settled());
  while (on_response.is_empty())
    ASSERT_TRUE(channel.process_next_instruction());
  ASSERT_FALSE(incoming->is_settled());
  on_response(OutgoingResponse::success(Variant::integer(18)));
  while (!incoming->is_settled())
    ASSERT_TRUE(channel.process_next_instruction());
  ASSERT_TRUE(Variant::integer(18) == incoming->peek_value(Variant::null()));
}

class EchoService : public plankton::rpc::Service {
public:
  void echo(RequestData &data, ResponseCallback response);
  void ping(RequestData &data, ResponseCallback response);
  EchoService();
};

EchoService::EchoService() {
  register_method("echo", tclib::new_callback(&EchoService::echo, this));
  register_method("ping", tclib::new_callback(&EchoService::ping, this));
}

void EchoService::echo(RequestData &data, ResponseCallback callback) {
  callback(OutgoingResponse::success(data[0]));
}

void EchoService::ping(RequestData &data, ResponseCallback callback) {
  callback(OutgoingResponse::success("pong"));
}

TEST(rpc, service) {
  EchoService echo;
  SharedRpcChannel channel(echo.handler());
  Variant args[1] = {43};
  OutgoingRequest req0(Variant::null(), "echo", 1, args);
  IncomingResponse inc0 = channel->send_request(&req0);
  OutgoingRequest req1(Variant::null(), "echo");
  IncomingResponse inc1 = channel->send_request(&req1);
  OutgoingRequest req2(Variant::null(), "ping");
  IncomingResponse inc2 = channel->send_request(&req2);
  while (!inc2->is_settled())
    channel.process_next_instruction();
  ASSERT_EQ(43, inc0->peek_value(Variant::null()).integer_value());
  ASSERT_TRUE(inc1->peek_value(10).is_null());
  ASSERT_TRUE(Variant::string("pong") == inc2->peek_value(10));
}

static opaque_t run_client(ByteBufferStream *down, ByteBufferStream *up) {
  EchoService echo;
  StreamServiceConnector client(down, up);
  ASSERT_TRUE(client.init(echo.handler()));
  ASSERT_TRUE(client.process_all_messages());
  ASSERT_TRUE(up->close());
  return o0();
}

TEST(rpc, async_service) {
  ByteBufferStream down(1024);
  ByteBufferStream up(1024);
  StreamServiceConnector server(&up, &down);
  NativeThread client(new_callback(run_client, &down, &up));
  ASSERT_TRUE(client.start());
  ASSERT_TRUE(server.init(empty_callback()));

  Variant arg = 54;
  OutgoingRequest req(Variant::null(), "echo", 1, &arg);
  IncomingResponse inc = server.socket()->send_request(&req);
  ASSERT_TRUE(down.close());
  ASSERT_TRUE(server.process_all_messages());
  ASSERT_TRUE(client.join(NULL));
  ASSERT_TRUE(inc->is_fulfilled());
  ASSERT_EQ(54, inc->peek_value(Variant::null()).integer_value());
}
