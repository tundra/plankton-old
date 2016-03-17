//- Copyright 2015 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "bytestream.hh"

#include "async/promise-inl.hh"
#include "marshal-inl.hh"
#include "rpc.hh"
#include "sync/thread.hh"

using namespace plankton;
using namespace plankton::rpc;
using namespace tclib;

ByteBufferStream::ByteBufferStream(uint32_t capacity)
  : capacity_(capacity)
  , next_read_cursor_(0)
  , next_write_cursor_(0)
  , readable_(0)
  , writable_(capacity)
  , buffer_(new Entry[capacity]) {
}

bool ByteBufferStream::initialize() {
  return readable_.initialize()
      && writable_.initialize()
      && buffer_mutex_.initialize();
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
