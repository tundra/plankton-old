//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "plankton-binary.hh"
#include "stdc.h"
#include "utils-inl.hh"
#include <new>

namespace plankton {

BinaryWriter::BinaryWriter()
  : bytes_(NULL)
  , size_(0) { }

BinaryWriter::~BinaryWriter() {
  delete[] bytes_;
  bytes_ = NULL;
}

class BinaryWriterImpl : public BinaryImplUtils {
public:
  // Write the given value to the stream.
  void encode(variant_t value);

  // Flush the contents of the stream, storing them in the fields of the given
  // writer.
  void flush(BinaryWriter *writer);

private:
  // Writes a single byte to the stream.
  void write_byte(uint8_t value);

  void write_int64(int64_t value);

  void write_uint64(uint64_t value);

  Buffer<uint8_t> bytes_;
};

void BinaryWriterImpl::flush(BinaryWriter *writer) {
  writer->size_ = bytes_.length();
  writer->bytes_ = bytes_.release();
}

void BinaryWriterImpl::encode(variant_t value) {
  switch (value.type()) {
    case variant_t::vtBool:
      write_byte(value.bool_value() ? boTrue : boFalse);
      break;
    case variant_t::vtInteger:
      write_byte(boInteger);
      write_int64(value.integer_value());
      break;
    case variant_t::vtNull:
    default:
      write_byte(boNull);
      break;
  }
}

void BinaryWriterImpl::write_byte(uint8_t value) {
  bytes_.add(value);
}

void BinaryWriter::write(variant_t value) {
  BinaryWriterImpl impl;
  impl.encode(value);
  impl.flush(this);
}

class BinaryReaderImpl : public BinaryImplUtils {
public:
  BinaryReaderImpl(const void *data, size_t size, BinaryReader *reader);

  bool decode(variant_t *result_out);

private:
  bool has_more() { return cursor_ < size_; }

  uint8_t current() { return data_[cursor_]; }

  uint8_t read_byte() { return data_[cursor_++]; }

  uint64_t read_uint64();

  int64_t read_int64();

  bool decode_integer(variant_t *result_out);

  // Succeeds parsing of some expression, returning true.
  bool succeed(variant_t value, variant_t *out);

  size_t size_;
  size_t cursor_;
  const uint8_t *data_;
  BinaryReader *reader_;
};

BinaryReaderImpl::BinaryReaderImpl(const void *data, size_t size, BinaryReader *reader)
  : size_(size)
  , cursor_(0)
  , data_(static_cast<const uint8_t*>(data))
  , reader_(reader) { }

bool BinaryReaderImpl::decode(variant_t *result_out) {
  if (!has_more())
    return false;
  uint8_t opcode = read_byte();
  switch (opcode) {
    case boTrue:
      return succeed(variant_t::yes(), result_out);
    case boFalse:
      return succeed(variant_t::no(), result_out);
    case boNull:
      return succeed(variant_t::null(), result_out);
    case boInteger:
      return decode_integer(result_out);
    default:
      return false;
  }
}

bool BinaryReaderImpl::decode_integer(variant_t *result_out) {
  int64_t value = read_int64();
  return succeed(variant_t::integer(value), result_out);
}

void BinaryWriterImpl::write_int64(int64_t value) {
  uint64_t zigzag = (value << 1) ^ (value >> 63);
  write_uint64(zigzag);
}

int64_t BinaryReaderImpl::read_int64() {
  uint64_t zigzag = read_uint64();
  return (zigzag >> 1) ^ (-(zigzag & 1));
}

// The wire encoding of unsigned integers is similar to protobuf varints with
// a slight twist. You might call them biased varints. Basically it's a sequence
// of bytes where the bottom 7 bits give 7 bits of the value and the top bit
// indicates whether there are more bytes coming. The order is backwards: the
// first byte holds the least significant 7 bits, etc.
//
// The problem with using this format directly is that it allows leading zeros,
// so these:
//
//   0x00
//   0x80 0x00
//   0x80 0x80 0x80 0x00
//
// are all valid representations of the same value, 0. This means that you
// either have to declare some encodings invalid or live with having multiple
// valid representations of the same number. It also means that you can make
// fewer assumptions about the value represented based on the length of the
// encoding. It would be really nice if each number had a unique representation
// and to accomplish this, instead of concatenating the payloads directly we add
// an implicit 1 to the payload of each byte, except the first one. This means
// that the examples from before now correspond to different values,
//
//   0x00 -> 0
//   0x80 0x00 -> 128 (= 2^7)
//   0x80 0x00 -> 16512 (= 2^7 + 2^14)
//   0x80 0x80 0x80 0x00 -> 2113664 (= 2^7 + 2^14 + 2^21)
//
// This is also slightly more space efficient -- without the bias two bytes will
// hold up to 16383, with the bias it's 16511, but that's in the order of less
// than 1% so it hardly matters.
uint64_t BinaryReaderImpl::read_uint64() {
  if (!has_more())
    return 0;
  uint8_t next = read_byte();
  if (next >= 0x80) {
    uint64_t result = (next & 0x7F);
    uint64_t offset = 7;
    while (has_more()) {
      uint8_t next = read_byte();
      uint64_t payload = ((next & 0x7F) + 1);
      result = result + (payload << offset);
      if (next < 0x80)
        return result;
      offset += 7;
    }
    return result;
  } else {
    return next;
  }
}

void BinaryWriterImpl::write_uint64(uint64_t value) {
  if (value >= 0x80) {
    write_byte((value & 0x7F) | 0x80);
    uint64_t current = (value >> 7) - 1;
    while (current >= 0x80) {
      write_byte((current & 0x7F) | 0x80);
      current = (current >> 7) - 1;
    }
    write_byte(current);
  } else {
    write_byte(value);
  }
}

bool BinaryReaderImpl::succeed(variant_t value, variant_t *out) {
  *out = value;
  return true;
}

BinaryReader::BinaryReader(arena_t *arena)
  : arena_(arena) { }

variant_t BinaryReader::parse(const void *data, size_t size) {
  BinaryReaderImpl decoder(data, size, this);
  variant_t result;
  decoder.decode(&result);
  return result;
}

} // namespace plankton
