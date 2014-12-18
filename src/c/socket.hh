//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// Plankton sockets and streams.

#ifndef _SOCKET_HH
#define _SOCKET_HH

#include "io/file.hh"
#include "variant.hh"
#include "plankton.hh"
#include "callback.hh"

// TODO: this needs to be changed to work with windows and probably across gcc
//   versions too.
#define _GLIBCXX_PERMIT_BACKWARD_HASH
#include <hash_map>
#define hash_map_alias __gnu_cxx::hash_map

namespace plankton {

static const byte_t kSetDefaultStringEncoding = 1;
static const byte_t kSendValue = 2;

class OutputSocket {
public:
  // Create a new output socket that writes to the given stream.
  OutputSocket(tclib::IoStream *dest);

  // Write the stream header.
  void init();

  // Sets the default encoding charset to use when encoding strings.
  void set_default_string_encoding(pton_charset_t value);

  // Sends the given value to the default stream.
  void send_value(Variant value, Variant stream_id);

private:
  // Writes the given raw data to the destination.
  void write_blob(byte_t *data, size_t size);

  // Serializes and writes the given value.
  void write_value(Variant value);

  // Writes a single byte to the destination.
  void write_byte(byte_t value);

  // Writes a varint encoded uint64 to the destination.
  void write_uint64(uint64_t value);

  // Writes 0s until the total number of bytes written is a multiple of 8.
  void write_padding();

  tclib::IoStream *dest_;
  size_t cursor_;
};

// The raw binary data associated with a message sent on a stream.
class MessageData {
public:
  MessageData(byte_t *data, size_t size)
    : data_(data)
    , size_(size) { }

  ~MessageData() { delete[] data_; }

  // Returns the raw message data.
  byte_t *data() { return data_; }

  // Returns the size in bytes of the message data.
  size_t size() { return size_; }

private:
  byte_t *data_;
  size_t size_;
};

// Utility class that wraps a binary stream id and adds the functionality you
// need in order to use one as the key in a hash map.
class StreamId {
public:
  // Creates a new stream id for the stream with the given binary key.
  StreamId(byte_t *raw_key, size_t key_size, bool owns_key);

  // Returns true iff this and the given stream id wrap identical binary keys.
  bool operator==(const StreamId &that) const;

  // Returns a hash of the underlying key.
  size_t hash_code() const { return hash_code_; }

  // Helper class that tells the hash map how to hash ids.
  class Hasher {
  public:
    size_t operator()(const StreamId &id) const { return id.hash_code(); }
  };

  // Stream ids are passed around by value so they don't need a destructor.
  // Sometimes though we need to be sure the underlying data is disposed and
  // that's what this method does.
  void dispose();

private:
  byte_t *raw_key_;
  size_t key_size_;
  size_t hash_code_;
  bool owns_key_;
};

// An input stream is an abstract type that receives data received through a
// socket.
class InputStream {
public:
  InputStream(StreamId id) : id_(id) { }

  virtual ~InputStream() { }

  // Called by the socket when a new value with this stream as its destination
  // has been received. Ownership of the message is passed to this stream so
  // it is the stream's responsibility to destroy it once it's no longer needed.
  virtual void receive_block(MessageData *message) = 0;

private:
  StreamId id_;
};

// An input stream that buffers blocks as they come in and lets clients pull
// the messages one at a time. This is flexible and requires no knowledge on
// the part of the input socket about the clients but requires the client to
// be vigilant about processing messages as they come in.
class BufferInputStream : public InputStream {
public:
  BufferInputStream(StreamId id);

  // Buffer the next block.
  virtual void receive_block(MessageData *message);

  // Decodes and returns the next pending message, acquiring storage from the
  // given arena.
  Variant pull_message(Arena *arena);

private:
  std::vector<MessageData*> pending_messages_;
};

class InputSocket {
public:
  // Create a new input socket that fetches data from the given source.
  InputSocket(tclib::IoStream *src);

  // Free all the data associated with this socket, including all the streams.
  // Once this method is called it is no longer safe to use any of the streams
  // returned from this socket.
  ~InputSocket();

  // Read the stream header. Returns true iff the header is valid.
  bool init();

  // Reads and processes the next instruction from the file. This will either
  // cause the internal state of the socket to be updated or a value to be
  // delivered to a stream. Returns true iff an instruction was processed, false
  // if input was in valid or if there is no more input to fecth.
  bool process_next_instruction();

  // Returns the root stream for this socket.
  InputStream *root_stream();

private:
  // Reads the requested number of bytes from the source, storing them in the
  // given array.
  void read_blob(byte_t *dest, size_t size);

  // Reads and returns a single byte from the source.
  byte_t read_byte();

  // Reads a varint encoded unsigned 64-bit value.
  uint64_t read_uint64();

  // Reads data until the number of bytes read in total is a multiple of 8.
  void read_padding();

  // Reads the next block of data.
  byte_t *read_value(size_t *size_out);

  // The default stream factory function.
  static InputStream *new_default_stream(StreamId id);

  // Returns the id of the root stream.
  static StreamId root_id();

  // Returns the input stream with the given id or NULL if one couldn't be
  // found.
  InputStream *get_stream(StreamId id);

  typedef hash_map_alias<StreamId, InputStream*, StreamId::Hasher> StreamMap;

  tclib::IoStream *src_;
  size_t cursor_;
  tclib::callback_t<InputStream*(StreamId)> stream_factory_;
  StreamMap streams_;
};

} // namespace plankton

#endif // _SOCKET_HH
