//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// C++ implementation of the plankton serialization format.

#ifndef _PLANKTON_HH
#define _PLANKTON_HH

#include "stdc.h"
#include "variant.hh"

namespace plankton {

// Utility for encoding plankton data. For most uses you can use a BinaryWriter
// to encode a whole variant at a time, but in cases where data is represented
// in some other way you can use this to build custom encoding.
class Assembler {
public:
  Assembler() : assm_(pton_new_assembler()) { }
  ~Assembler() { pton_dispose_assembler(assm_); }

  // Writes an array header for an array with the given number of elements. This
  // must be followed immediately by the elements.
  bool begin_array(uint32_t length) { return pton_assembler_begin_array(assm_, length); }

  // Writes a map header for a map with the given number of mappings. This must be
  // followed immediately by the mappings, keys and values alternating.
  bool begin_map(uint32_t size) { return pton_assembler_begin_map(assm_, size); }

  // Writes an object header for an object with the given number of fields. This
  // must be followed immediately by the header and body of the object.
  bool begin_object(uint32_t fieldc) { return pton_assembler_begin_object(assm_, fieldc); }

  // Writes the given boolean value.
  bool emit_bool(bool value) { return pton_assembler_emit_bool(assm_, value); }

  // Writes the null value.
  bool emit_null() { return pton_assembler_emit_null(assm_); }

  // Writes an int64 with the given value.
  bool emit_int64(int64_t value) { return pton_assembler_emit_int64(assm_, value); }

  // Writes a string with the default encoding.
  bool emit_default_string(const char *chars, uint32_t length) {
    return pton_assembler_emit_default_string(assm_, chars, length);
  }

  // Writes a blob with the given contents.
  bool emit_blob(const void *data, uint32_t size) { return pton_assembler_emit_blob(assm_, data, size); }

  // Writes the header for a string with a custom encoding.
  bool begin_string_with_encoding(const void *chars, uint32_t length) {
    return pton_assembler_begin_string_with_encoding(assm_, chars, length);
  }

  // Writes an identity token.
  bool emit_id64(uint32_t size, uint64_t value) {
    return pton_assembler_emit_id64(assm_, size, value);
  }

  // Flushes the given assembler, writing the output into the given parameters.
  // The caller assumes ownership of the returned array and is responsible for
  // freeing it. This doesn't free the assembler, it must still be disposed with
  // pton_dispose_assembler.
  memory_block_t peek_code() { return pton_assembler_peek_code(assm_); }

private:
  pton_assembler_t *assm_;
};

// Utility for serializing variant values to plankton.
class BinaryWriter {
public:
  BinaryWriter();
  ~BinaryWriter();

  // Write the given value to this writer's internal buffer.
  void write(Variant value);

  // Returns the start of the buffer.
  uint8_t *operator*() { return bytes_; }

  // Returns the size in bytes of the data written to this writer's buffer.
  size_t size() { return size_; }

private:
  friend class VariantWriter;
  uint8_t *bytes_;
  size_t size_;
};

// An object that holds the representation of a variant as a 7-bit ascii string.
class TextWriter {
public:
  TextWriter();
  ~TextWriter();

  // Write the given variant to this asciigram.
  void write(Variant value);

  // After encoding, returns the string containing the encoded representation.
  const char *operator*() { return chars_; }

  // After encoding, returns the length of the string containing the encoded
  // representation.
  size_t length() { return length_; }

private:
  friend class TextWriterImpl;
  char *chars_;
  size_t length_;
};

// Utility for reading variant values from serialized data.
class BinaryReader {
public:
  // Creates a new reader that allocates values from the given arena.
  BinaryReader(Arena *arena);

  // Deserializes the given input and returns the result as a variant.
  Variant parse(const void *data, size_t size);

private:
  friend class BinaryReaderImpl;
  Arena *arena_;
};

// Utility for converting a plankton variant to a 7-bit ascii string.
class TextReader {
public:
  // Creates a new parser which uses the given arena for allocation.
  TextReader(Arena *arena);

  // Parse the given input, returning the value. If any errors occur the
  // has_failed() and offender() methods can be used to identify what the
  // problem was.
  Variant parse(const char *chars, size_t length);

  // Returns true iff the last parse failed.  If parse hasn't been called at all
  // returns false.
  bool has_failed() { return has_failed_; }

  // If has_failed() returns true this will return the offending character.
  char offender() { return offender_; }

private:
  friend class TextReaderImpl;
  Arena *arena_;
  bool has_failed_;
  char offender_;
};

} // namespace plankton

#endif // _PLANKTON_HH
