//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "plankton-binary.hh"
#include "stdc.h"
#include "utils-inl.hh"
#include <new>

using namespace plankton;

// Implementation of the type declared in the C header. This is the actual
// assembler implementation, the C++ wrapper delegates to this one.
struct pton_assembler_t : public BinaryImplUtils {
public:
  // Write the given value to the stream.
  void encode(variant_t value);

  // Flush the contents of the stream, storing them in the fields of the given
  // writer.
  void flush(BinaryWriter *writer);

  bool begin_array(uint32_t length);

  bool begin_map(uint32_t length);

  bool begin_object();

  bool begin_environment_reference();

  bool emit_bool(bool value);

  bool emit_null();

  bool emit_int64(int64_t value);

  bool emit_default_string(const char *chars, uint32_t length);

  bool begin_string_with_encoding(const void *chars, uint32_t length);

  bool emit_id64(uint32_t size, uint64_t value);

  bool emit_reference(uint64_t offset);

  memory_block_t peek_code();

private:
  // Write a single byte to the stream.
  bool write_byte(uint8_t value);

  // Write an untagged signed int64 varint.
  bool write_int64(int64_t value);

  // Write an untagged unsigned int64 varint.
  bool write_uint64(uint64_t value);

  Buffer<uint8_t> bytes_;
};

pton_assembler_t *pton_new_assembler() {
  return new pton_assembler_t();
}

bool pton_assembler_t::begin_array(uint32_t length) {
  return write_byte(boArray) && write_uint64(length);
}

bool pton_assembler_begin_array(pton_assembler_t *assm, uint32_t length) {
  return assm->begin_array(length);
}

bool pton_assembler_t::begin_map(uint32_t size) {
  return write_byte(boMap) && write_uint64(size);
}

bool pton_assembler_begin_map(pton_assembler_t *assm, uint32_t size) {
  return assm->begin_map(size);
}

bool pton_assembler_t::emit_bool(bool value) {
  return write_byte(value ? boTrue : boFalse);
}

bool pton_assembler_t::begin_object() {
  return write_byte(boObject);
}

bool pton_assembler_begin_object(pton_assembler_t *assm) {
  return assm->begin_object();
}

bool pton_assembler_t::begin_environment_reference() {
  return write_byte(boBeginEnvironmentReference);
}

bool pton_assembler_begin_environment_reference(pton_assembler_t *assm) {
  return assm->begin_environment_reference();
}

bool pton_assembler_emit_bool(pton_assembler_t *assm, bool value) {
  return assm->emit_bool(value);
}

bool pton_assembler_t::emit_null() {
  return write_byte(boNull);
}

bool pton_assembler_emit_null(pton_assembler_t *assm) {
  return assm->emit_null();
}

bool pton_assembler_t::emit_int64(int64_t value) {
  return write_byte(boInteger) && write_int64(value);
}

bool pton_assembler_emit_int64(pton_assembler_t *assm, int64_t value) {
  return assm->emit_int64(value);
}

bool pton_assembler_t::emit_default_string(const char *chars, uint32_t length) {
  write_byte(boDefaultString);
  write_uint64(length);
  bytes_.write(reinterpret_cast<const uint8_t*>(chars), length);
  return true;
}

bool pton_assembler_emit_default_string(pton_assembler_t *assm, const char *chars,
    uint32_t length) {
  return assm->emit_default_string(chars, length);
}

bool pton_assembler_t::begin_string_with_encoding(const void *chars, uint32_t length) {
  write_byte(boBeginStringWithEncoding);
  write_uint64(length);
  bytes_.write(static_cast<const uint8_t*>(chars), length);
  return true;
}

bool pton_assembler_begin_string_with_encoding(pton_assembler_t *assm,
    const void *chars, uint32_t length) {
  return assm->begin_string_with_encoding(chars, length);
}

bool pton_assembler_t::emit_id64(uint32_t size, uint64_t value) {
  write_byte(boId);
  switch (size) {
    case 64: case 32: case 16: case 8:
      write_byte(size >> 3);
      break;
    default:
      return false;
  }
  switch (size) {
    case 64: {
      bytes_.write(reinterpret_cast<uint8_t*>(&value), 8);
      break;
    }
    case 32: {
      uint32_t smaller_value = value;
      bytes_.write(reinterpret_cast<uint8_t*>(&smaller_value), 4);
      break;
    }
    case 16: {
      uint16_t smaller_value = value;
      bytes_.write(reinterpret_cast<uint8_t*>(&smaller_value), 2);
      break;
    }
    case 8: {
      uint8_t smaller_value = value;
      bytes_.write(reinterpret_cast<uint8_t*>(&smaller_value), 1);
      break;
    }
  }
  return true;
}

bool pton_assembler_emit_id64(pton_assembler_t *assm, uint32_t size,
    uint64_t value) {
  return assm->emit_id64(size, value);
}

bool pton_assembler_t::emit_reference(uint64_t offset) {
  return write_byte(boReference) && write_uint64(offset);
}

bool pton_assembler_emit_reference(pton_assembler_t *assm, uint64_t offset) {
  return assm->emit_reference(offset);
}

memory_block_t pton_assembler_t::peek_code() {
  return new_memory_block(*bytes_, bytes_.length());
}

memory_block_t pton_assembler_peek_code(pton_assembler_t *assm) {
  return assm->peek_code();
}

void pton_dispose_assembler(pton_assembler_t *assm) {
  delete assm;
}

bool pton_assembler_t::write_byte(uint8_t value) {
  bytes_.add(value);
  return true;
}

bool pton_assembler_t::write_int64(int64_t value) {
  uint64_t zigzag = (value << 1) ^ (value >> 63);
  return write_uint64(zigzag);
}

bool pton_assembler_t::write_uint64(uint64_t value) {
  uint64_t current = value;
  while (current >= 0x80) {
    write_byte((current & 0x7F) | 0x80);
    current = (current >> 7) - 1;
  }
  write_byte(current);
  return true;
}

namespace plankton {

BinaryWriter::BinaryWriter()
  : bytes_(NULL)
  , size_(0) { }

BinaryWriter::~BinaryWriter() {
  delete[] bytes_;
  bytes_ = NULL;
}

// Utility that holds the state used when encoding a variant as plankton. The
// difference between this and a BinaryWriter is that the binary writer's
// lifetime is controlled by the client, the variant writer is created to write
// one variant and then torn down.
class VariantWriter {
public:
  VariantWriter(Assembler *assm) : assm_(assm) { }

  // Write the given value to the stream.
  void encode(variant_t value);

  // Flush the contents of the stream, storing them in the fields of the given
  // writer.
  void flush(BinaryWriter *writer);

  void encode_array(array_t value);

  void encode_string(string_t value);

  void encode_map(map_t value);

private:
  Assembler *assm_;
  Assembler *assm() { return assm_; }
};

void VariantWriter::flush(BinaryWriter *writer) {
  memory_block_t live = assm()->peek_code();
  writer->size_ = live.size;
  writer->bytes_ = new uint8_t[live.size];
  memcpy(writer->bytes_, live.memory, live.size);
}

void VariantWriter::encode(variant_t value) {
  switch (value.type()) {
    case PTON_ARRAY:
      encode_array(array_t(value));
      break;
    case PTON_STRING:
      encode_string(string_t(value));
      break;
    case PTON_MAP:
      encode_map(map_t(value));
      break;
    case PTON_BOOL:
      assm()->emit_bool(value.bool_value());
      break;
    case PTON_INTEGER:
      assm()->emit_int64(value.integer_value());
      break;
    case PTON_ID:
      assm()->emit_id64(value.id_size(), value.id64_value());
      break;
    case PTON_NULL:
    default:
      assm()->emit_null();
      break;
  }
}

void VariantWriter::encode_string(string_t value) {
  size_t length = value.length();
  variant_t encoding = value.encoding();
  if (encoding == variant_t::default_string_encoding()) {
    assm()->emit_default_string(value.chars(), length);
  } else {
    assm()->begin_string_with_encoding(value.chars(), length);
    encode(encoding);
  }
}

void VariantWriter::encode_array(array_t value) {
  size_t length = value.length();
  assm()->begin_array(length);
  for (size_t i = 0; i < length; i++)
    encode(value[i]);
}

void VariantWriter::encode_map(map_t value) {
  size_t size = value.size();
  assm()->begin_map(size);
  map_iterator_t iter = value.map_iter();
  while (iter.has_next()) {
    variant_t key;
    variant_t value;
    iter.advance(&key, &value);
    encode(key);
    encode(value);
  }
}


void BinaryWriter::write(variant_t value) {
  Assembler assm;
  VariantWriter writer(&assm);
  writer.encode(value);
  writer.flush(this);
}

class BinaryReaderImpl : public BinaryImplUtils {
public:
  BinaryReaderImpl(const void *data, size_t size, BinaryReader *reader);

  bool decode(variant_t *result_out);

private:
  // Returns true iff there are more bytes to return.
  bool has_more() { return cursor_ < size_; }

  // Read an array's payload.
  bool decode_array(uint64_t length, variant_t *result_out);

  // Read a map's payload.
  bool decode_map(uint64_t size, variant_t *result_out);

  // Convert default-encoding string data into a string variant.
  bool decode_default_string(pton_instr_t *instr, variant_t *result_out);

  // Convert a custom-encoding string data into a string variant.
  bool decode_string_with_encoding(pton_instr_t *instr, variant_t *result_out);

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

// Utility for decoding an individual instruction.
class InstrDecoder {
public:
  InstrDecoder(const uint8_t *data, size_t size)
    : data_(data)
    , size_(size)
    , cursor_(0) { }

  // Returns true iff there are more bytes to return.
  bool has_more() { return cursor_ < size_; }

  // Returns true iff this decode has data enough that a block of the given size
  // can be read.
  bool has_data(size_t required) { return (cursor_ + required) <= size_; }

  // Advances past and returns the current byte.
  uint8_t read_byte() { return data_[cursor_++]; }

  bool read_bytes(uint8_t *dest, size_t size) {
    if (!has_data(size))
      return false;
    memcpy(dest, data_ + cursor_, size);
    cursor_ += size;
    return true;
  }

  bool decode(pton_instr_t *instr_out);

  bool decode_int64(int64_t *result_out);

  bool decode_uint64(uint64_t *result_out);

private:
  const uint8_t *data_;
  size_t size_;
  size_t cursor_;
};

bool InstrDecoder::decode(pton_instr_t *instr_out) {
  if (!has_more())
    return false;
  uint8_t opcode = read_byte();
  switch (opcode) {
    case BinaryImplUtils::boInteger:
      if (!decode_int64(&instr_out->payload.int64_value))
        return false;
      instr_out->opcode = pton_instr_t::PTON_OPCODE_INT64;
      break;
    case BinaryImplUtils::boDefaultString: {
      uint64_t length = 0;
      if (!decode_uint64(&length))
        return false;
      if (!has_data(length))
        return false;
      instr_out->opcode = pton_instr_t::PTON_OPCODE_DEFAULT_STRING;
      instr_out->payload.default_string_data.length = length;
      instr_out->payload.default_string_data.contents = data_ + cursor_;
      cursor_ += length;
      break;
    }
    case BinaryImplUtils::boBeginStringWithEncoding: {
      uint64_t length = 0;
      if (!decode_uint64(&length))
        return false;
      if (!has_data(length))
        return false;
      instr_out->opcode = pton_instr_t::PTON_OPCODE_BEGIN_STRING_WITH_ENCODING;
      instr_out->payload.string_with_encoding_data.length = length;
      instr_out->payload.string_with_encoding_data.contents = data_ + cursor_;
      cursor_ += length;
      break;
    }
    case BinaryImplUtils::boArray:
      if (!decode_uint64(&instr_out->payload.array_length))
        return false;
      instr_out->opcode = pton_instr_t::PTON_OPCODE_BEGIN_ARRAY;
      break;
    case BinaryImplUtils::boMap:
      if (!decode_uint64(&instr_out->payload.map_size))
        return false;
      instr_out->opcode = pton_instr_t::PTON_OPCODE_BEGIN_MAP;
      break;
    case BinaryImplUtils::boNull:
      instr_out->opcode = pton_instr_t::PTON_OPCODE_NULL;
      instr_out->size = 1;
      break;
    case BinaryImplUtils::boTrue:
    case BinaryImplUtils::boFalse:
      instr_out->opcode = pton_instr_t::PTON_OPCODE_BOOL;
      instr_out->payload.bool_value = (opcode == BinaryImplUtils::boTrue);
      break;
    case BinaryImplUtils::boObject:
      instr_out->opcode = pton_instr_t::PTON_OPCODE_BEGIN_OBJECT;
      break;
    case BinaryImplUtils::boReference:
      if (!decode_uint64(&instr_out->payload.reference_offset))
        return false;
      instr_out->opcode = pton_instr_t::PTON_OPCODE_REFERENCE;
      break;
    case BinaryImplUtils::boBeginEnvironmentReference:
      instr_out->opcode = pton_instr_t::PTON_OPCODE_BEGIN_ENVIRONMENT_REFERENCE;
      break;
    case BinaryImplUtils::boId: {
      if (!has_more())
        return false;
      uint8_t size = read_byte() << 3;
      uint64_t value = 0;
      switch (size) {
        case 64: {
          if (!read_bytes(reinterpret_cast<uint8_t*>(&value), 8))
            return false;
          break;
        }
        case 32: {
          uint32_t smaller_value = 0;
          if (!read_bytes(reinterpret_cast<uint8_t*>(&smaller_value), 4))
            return false;
          value = smaller_value;
          break;
        }
        case 16: {
          uint16_t smaller_value = 0;
          if (!read_bytes(reinterpret_cast<uint8_t*>(&smaller_value), 2))
            return false;
          value = smaller_value;
          break;
        }
        case 8: {
          value = read_byte();
          break;
        }
        default:
          return false;
      }
      instr_out->payload.id64.value = value;
      instr_out->payload.id64.size = size;
      instr_out->opcode = pton_instr_t::PTON_OPCODE_ID64;
      break;
    }
    default:
      fprintf(stderr, "Unknown instruction %i\n", opcode);
      fflush(stderr);
      return false;
  }
  instr_out->size = cursor_;
  return true;
}

bool InstrDecoder::decode_int64(int64_t *result_out) {
  uint64_t zigzag = 0;
  if (!decode_uint64(&zigzag))
    return false;
  *result_out = (zigzag >> 1) ^ (-(zigzag & 1));
  return true;
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
//   0x80 0x80 0x00 -> 16512 (= 2^7 + 2^14)
//   0x80 0x80 0x80 0x00 -> 2113664 (= 2^7 + 2^14 + 2^21)
//
// This is also slightly more space efficient -- without the bias two bytes will
// hold up to 16383, with the bias it's 16511, but that's in the order of less
// than 1% so it hardly matters.
bool InstrDecoder::decode_uint64(uint64_t *result_out) {
  if (!has_more())
    return false;
  uint8_t next = read_byte();
  uint64_t result = (next & 0x7F);
  uint64_t offset = 7;
  while (next >= 0x80) {
    if (!has_more())
      return false;
    next = read_byte();
    uint64_t payload = ((next & 0x7F) + 1);
    result = result + (payload << offset);
    offset += 7;
  }
  *result_out = result;
  return true;
}


bool BinaryReaderImpl::decode(variant_t *result_out) {
  if (!has_more())
    return false;
  pton_instr_t instr;
  if (!pton_decode_next_instruction(data_ + cursor_, size_ - cursor_, &instr))
    return false;
  cursor_ += instr.size;
  switch (instr.opcode) {
    case pton_instr_t::PTON_OPCODE_INT64:
      return succeed(variant_t::integer(instr.payload.int64_value), result_out);
    case pton_instr_t::PTON_OPCODE_DEFAULT_STRING:
      return decode_default_string(&instr, result_out);
    case pton_instr_t::PTON_OPCODE_BEGIN_STRING_WITH_ENCODING:
      return decode_string_with_encoding(&instr, result_out);
    case pton_instr_t::PTON_OPCODE_BEGIN_ARRAY:
      return decode_array(instr.payload.array_length, result_out);
    case pton_instr_t::PTON_OPCODE_BEGIN_MAP:
      return decode_map(instr.payload.map_size, result_out);
    case pton_instr_t::PTON_OPCODE_NULL:
      return succeed(variant_t::null(), result_out);
    case pton_instr_t::PTON_OPCODE_BOOL:
      return succeed(variant_t::boolean(instr.payload.bool_value), result_out);
    case pton_instr_t::PTON_OPCODE_ID64:
      return succeed(variant_t::id(instr.payload.id64.size, instr.payload.id64.value),
          result_out);
    default:
      return false;
  }
}

bool BinaryReaderImpl::decode_default_string(pton_instr_t *instr, variant_t *result_out) {
  const uint8_t *chars = instr->payload.default_string_data.contents;
  uint64_t size = instr->payload.default_string_data.length;
  string_t result = reader_->arena_->new_string(size);
  memcpy(result.mutable_chars(), chars, size);
  result.ensure_frozen();
  return succeed(result, result_out);
}

bool BinaryReaderImpl::decode_string_with_encoding(pton_instr_t *instr, variant_t *result_out) {
  variant_t encoding;
  if (!decode(&encoding))
    return false;
  const uint8_t *chars = instr->payload.string_with_encoding_data.contents;
  uint64_t size = instr->payload.string_with_encoding_data.length;
  string_t result = reader_->arena_->new_string(size, encoding);
  memcpy(result.mutable_chars(), chars, size);
  result.ensure_frozen();
  return succeed(result, result_out);
}

bool BinaryReaderImpl::decode_array(uint64_t length, variant_t *result_out) {
  array_t result = reader_->arena_->new_array(length);
  for (size_t i = 0; i < length; i++) {
    variant_t elm;
    if (!decode(&elm))
      return false;
    result.add(elm);
  }
  result.ensure_frozen();
  return succeed(result, result_out);
}

bool BinaryReaderImpl::decode_map(uint64_t size, variant_t *result_out) {
  map_t result = reader_->arena_->new_map();
  for (size_t i = 0; i < size; i++) {
    variant_t key;
    if (!decode(&key))
      return false;
    variant_t value;
    if (!decode(&value))
      return false;
    result.set(key, value);
  }
  result.ensure_frozen();
  return succeed(result, result_out);
}

bool BinaryReaderImpl::succeed(variant_t value, variant_t *out) {
  *out = value;
  return true;
}

BinaryReader::BinaryReader(pton_arena_t *arena)
  : arena_(arena) { }

variant_t BinaryReader::parse(const void *data, size_t size) {
  BinaryReaderImpl decoder(data, size, this);
  variant_t result;
  decoder.decode(&result);
  return result;
}

} // namespace plankton

bool pton_decode_next_instruction(const uint8_t *code, size_t size, pton_instr_t *instr_out) {
  InstrDecoder in(code, size);
  return in.decode(instr_out);
}
