//- Copyright 2016 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#ifndef _BYTESTREAM_HH
#define _BYTESTREAM_HH

#include "io/stream.hh"
#include "sync/mutex.hh"
#include "sync/semaphore.hh"

namespace tclib {

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
  bool initialize();
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

} // namespace tclib

#endif // _BYTESTREAM_HH
