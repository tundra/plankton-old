//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// Plankton sockets and streams.

#ifndef _SOCKET_HH
#define _SOCKET_HH

#include "c/stdhashmap.hh"
#include "io/file.hh"
#include "marshal.hh"
#include "plankton.hh"
#include "utils/callback.hh"
#include "variant.hh"

namespace plankton {

static const byte_t kSetDefaultStringEncoding = 1;
static const byte_t kSendValue = 2;

class OutputSocket {
public:
  // Create a new output socket that writes to the given stream.
  OutputSocket(tclib::OutStream *dest);

  // Write the stream header.
  void init();

  // Sets the default encoding charset to use when encoding strings. This must
  // be done before init is called. The default encoding is utf-8.
  bool set_default_string_encoding(pton_charset_t value);

  // Sends the given value to the default stream.
  void send_value(Variant value, Variant stream_id = Variant::null());

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

  tclib::OutStream *dest_;
  size_t cursor_;
  pton_charset_t default_encoding_;
  bool has_been_inited_;
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

  // Returns true iff the binary key is lexically less than the given one.
  bool operator<(const StreamId &that) const;

  // Returns a hash of the underlying key.
  size_t hash_code() const { return hash_code_; }

  // Helper class that tells the hash map how to hash ids.
  class Hasher {
  public:
    size_t operator()(const StreamId &id) const { return id.hash_code(); }

    // See http://msdn.microsoft.com/en-us/library/1s1byw77.aspx.
    static const size_t bucket_size = 4;

    // The MSVC hash map needs the keys to be ordered using this operator. Wut?
    bool operator()(const StreamId &a, const StreamId &b) { return a < b; }
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

// Data used when initializing new streams.
class InputStreamConfig {
public:
  InputStreamConfig(StreamId id, TypeRegistry *default_type_registry)
    : id_(id)
    , default_type_registry_(default_type_registry) { }
  StreamId id() { return id_; }
  TypeRegistry *default_type_registry() { return default_type_registry_; }
private:
  StreamId id_;
  TypeRegistry *default_type_registry_;
};

// An input stream is an abstract type that receives data received through a
// socket.
class InputStream {
public:
  InputStream(InputStreamConfig *config) : id_(config->id()) { }

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
  BufferInputStream(InputStreamConfig *config);

  // Buffer the next block.
  virtual void receive_block(MessageData *message);

  // Sets the type registry to use when decoding values on this stream.
  void set_type_registry(TypeRegistry *value) { type_registry_ = value; }

  // Decodes and returns the next pending message, acquiring storage from the
  // given arena.
  Variant pull_message(Factory *factory);

  // Returns true iff there are no messages to pull.
  bool is_empty() { return pending_messages_.empty(); }

private:
  std::vector<MessageData*> pending_messages_;
  TypeRegistry *type_registry_;
};

// Data associated with a pre-parsed message received through a socket.
class ParsedMessage {
public:
  ParsedMessage(VariantOwner *owner, Variant value)
    : owner_(owner)
    , value_(value) { }

  // Yields the object that owns the parsed value.
  VariantOwner *owner() { return owner_; }

  // Yields the parsed value. The value is owned by the object available through
  // owner().
  Variant value() { return value_; }

private:
  VariantOwner *owner_;
  Variant value_;
};

// An input stream that parses and handles messages immediately.
class PushInputStream : public InputStream {
public:
  typedef tclib::callback_t<void(ParsedMessage*)> MessageAction;

  // Creates a new input stream that performs the given action on each message
  // it receives. The variant value passed to the action is valid during the
  // call only, the behavior of variants past the end of the call is undefined.
  PushInputStream(InputStreamConfig *config, MessageAction action = tclib::empty_callback());

  // Static method for creating push input streams that conform to the type
  // expected for an input stream factory.
  static InputStream *new_instance(InputStreamConfig *config);

  // Sets the type registry to use when decoding values on this stream.
  void set_type_registry(TypeRegistry *value) { type_registry_ = value; }

  virtual void receive_block(MessageData *message);

  // Adds an action to be performed when messages are received. This new action
  // will be performed when the actions that have already been registered have
  // been performed.
  void add_action(MessageAction action);

private:
  std::vector<MessageAction> actions_;
  TypeRegistry *type_registry_;
};

class InputSocket {
public:
  typedef tclib::callback_t<InputStream*(InputStreamConfig*)> InputStreamFactory;

  // The outcome of processing an instruction.
  class ProcessInstrStatus {
  public:
    ProcessInstrStatus() : is_error_(false) { }
    ProcessInstrStatus(bool is_error) : is_error_(is_error) { }

    // Did an error occur while processing this instruction?
    bool is_error() { return is_error_; }

  private:
    bool is_error_;
  };

  // Create a new input socket that fetches data from the given source.
  InputSocket(tclib::InStream *src);

  // Sets the factory used to create input streams. If you want to set the
  // stream factory you have to do it before calling init(). Returns true if
  // setting succeeded, false if not.
  bool set_stream_factory(InputStreamFactory factory);

  void set_default_type_registry(TypeRegistry *value) { default_type_registry_ = value; }

  // Free all the data associated with this socket, including all the streams.
  // Once this method is called it is no longer safe to use any of the streams
  // returned from this socket.
  ~InputSocket();

  // Read the stream header. Returns true iff the header is valid.
  bool init();

  // Reads and processes the next instruction from the input. This will either
  // cause the internal state of the socket to be updated or a value to be
  // delivered to a stream. Returns true iff an instruction was processed, false
  // if the end of the input was reached. If the out-argument is non-null the
  // status of processing the instruction is stored there.
  bool process_next_instruction(ProcessInstrStatus *status_out);

  // Keeps processing instructions until the end of the input is reached.
  // Returns true if all input was valid.
  bool process_all_instructions();

  // Returns the root stream for this socket. This stream was produced by this
  // socket's stream factory.
  InputStream *root_stream();

private:
  // Reads the requested number of bytes from the source, storing them in the
  // given array.
  void read_blob(byte_t *dest, size_t size, bool *at_eof_out);

  // Reads and returns a single byte from the source.
  byte_t read_byte(bool *at_eof_out);

  // Reads a varint encoded unsigned 64-bit value.
  uint64_t read_uint64(bool *at_eof_out);

  // Reads a varint encoded unsigned 32-bit value.
  uint32_t read_uint32(bool *at_eof_out);

  // Reads data until the number of bytes read in total is a multiple of 8.
  void read_padding(bool *at_eof_out);

  // Reads the next block of data.
  byte_t *read_value(size_t *size_out, bool *at_eof_out);

  // The default stream factory function.
  static InputStream *new_default_stream(InputStreamConfig *config);

  // Returns the id of the root stream.
  static StreamId root_id();

  // Returns the input stream with the given id or NULL if one couldn't be
  // found.
  InputStream *get_stream(StreamId id);

  typedef platform_hash_map<StreamId, InputStream*, StreamId::Hasher> StreamMap;

  tclib::InStream *src_;
  bool has_been_inited_;
  size_t cursor_;
  InputStreamFactory stream_factory_;
  StreamMap streams_;
  TypeRegistry *default_type_registry_;
};

} // namespace plankton

#endif // _SOCKET_HH
