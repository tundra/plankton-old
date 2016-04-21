//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// C bindings of the plankton serialization format.

#ifndef _PLANKTON_H
#define _PLANKTON_H

#include "c/stdc.h"
#include "utils/alloc.h"

typedef struct pton_arena_array_t pton_arena_array_t;
typedef struct pton_arena_blob_t pton_arena_blob_t;
typedef struct pton_arena_map_t pton_arena_map_t;
typedef struct pton_arena_native_t pton_arena_native_t;
typedef struct pton_arena_seed_t pton_arena_seed_t;
typedef struct pton_arena_string_t pton_arena_string_t;
typedef struct pton_arena_t pton_arena_t;
typedef struct pton_arena_value_t pton_arena_value_t;
typedef struct pton_assembler_t pton_assembler_t;
typedef struct pton_sink_t pton_sink_t;
typedef struct pton_command_line_t pton_command_line_t;
typedef struct pton_command_line_reader_t pton_command_line_reader_t;
typedef struct pton_syntax_error_t pton_syntax_error_t;
typedef struct pton_native_info_t pton_native_info_t;

// The different types of variants. The values are the corresponding
// representation tags downshifted by 4.
typedef enum pton_type_t {
  PTON_INTEGER = 0x01,
  PTON_STRING = 0x02,
  PTON_BLOB = 0x03,
  PTON_NULL = 0x04,
  PTON_BOOL = 0x05,
  PTON_ARRAY = 0x06,
  PTON_MAP = 0x07,
  PTON_ID = 0x08,
  PTON_SEED = 0x09,
  PTON_NATIVE = 0x0A
} pton_type_t;

// The charsets supported by plankton.
//
// TODO: only utf-8 is actually supported in c and a decision needs to be made
//   about what "support" means -- just keeping the raw bytes and carrying the
//   encoding along with them, or performing an actual conversion.
typedef enum pton_charset_t {
  // Indicates no valid charset. The value 0 is reserved in the enumeration so
  // it should be safe to use here.
  PTON_CHARSET_NONE = 0,
  PTON_CHARSET_US_ASCII = 3,
  PTON_CHARSET_SHIFT_JIS = 17,
  PTON_CHARSET_UTF_8 = 106
} pton_charset_t;

// A value that encodes a value and the value's type. It provides a uniform
// interface that abstracts over all the different types that can be encoded
// to and decoded from plankton.
typedef struct {
  // The header of the variant. This part is the same in all variants.
  struct pton_variant_header_t {
    // The tag that identifies what kind of variant we're dealing with.
    enum pton_variant_repr_tag_t {
        PTON_REPR_INT64 = 0x10,
        PTON_REPR_EXTN_STRING = 0x20,
        PTON_REPR_ARNA_STRING = 0x21,
        PTON_REPR_EXTN_BLOB = 0x30,
        PTON_REPR_ARNA_BLOB = 0x31,
        PTON_REPR_NULL = 0x40,
        PTON_REPR_TRUE = 0x50,
        PTON_REPR_FALSE = 0x51,
        PTON_REPR_ARNA_ARRAY = 0x60,
        PTON_REPR_ARNA_MAP = 0x70,
        PTON_REPR_INLN_ID = 0x80,
        PTON_REPR_ARNA_SEED = 0x90,
        PTON_REPR_ARNA_NATIVE = 0xA0,
        PTON_REPR_EXTN_NATIVE = 0xA1
    } repr_tag_ UNLESS_MSVC(: 8);
    // A tag used to identify the version of plankton that produced this value.
    // All variants returned from a binary plankton implementation will have the
    // same version tag, and the version of all arguments will be checked
    // against that tag. If a function is called with the wrong version the call
    // will abort immediately. This can be used to trap binary incompatibility
    // by bumping the version when making incompatible changes to variant
    // representation. It is also useful in trapping invalid or uninitialized
    // data viewed as variants.
    uint8_t binary_version_ UNLESS_MSVC(: 8);
    // A length or size field. Only used by some variants but it wastes space
    // on 64 bits to put it in the payload. Note that this field can not be
    // used the length of a value whose length can change (arrays for instance).
    uint32_t length_ UNLESS_MSVC(: 32);
  } header_;

  // The contents of a variant. This is the part that changes depending on the
  // type of the variant.
  union pton_variant_payload_t {
    int64_t as_int64_;
    uint64_t as_inline_id_;
    pton_arena_value_t *as_arena_value_;
    pton_arena_array_t *as_arena_array_;
    pton_arena_map_t *as_arena_map_;
    pton_arena_native_t *as_arena_native_;
    pton_arena_seed_t *as_arena_seed_;
    pton_arena_string_t *as_arena_string_;
    pton_arena_blob_t *as_arena_blob_;
    const void *as_external_blob_data_;
    const char *as_external_string_chars_;
    pton_native_info_t *as_external_native_;
  } payload_;

} pton_variant_t;

// Iterator for scanning through the entries of a plankton map.
typedef struct {
  pton_arena_map_t *data;
  uint32_t cursor;
} pton_map_iter_t;

// Returns a variant representing null.
pton_variant_t pton_null();

// Returns a variant representing the boolean true.
pton_variant_t pton_true();

// Returns a variant representing the boolean false.
pton_variant_t pton_false();

// Returns a variant representing the given boolean.
pton_variant_t pton_bool(bool value);

// Returns a variant representing an integer with the given value.
pton_variant_t pton_integer(int64_t value);

// Returns a variant representing a 64-bit identity token.
pton_variant_t pton_id64(uint64_t value);

// Returns a variant representing a 32-bit identity token.
pton_variant_t pton_id32(uint32_t value);

// Returns a variant representing an identity token.
pton_variant_t pton_id(uint32_t size, uint64_t value);

// Constructor for string-valued variants. Note that the variant does not take
// ownership of the string so it must stay alive as long as the variant does.
// Use an arena to create a variant that does take ownership.
pton_variant_t pton_string(const char *chars, uint32_t length);

// Constructor for string-valued variants. This function uses strlen to
// determine the length of the string; use pton_string instead to pass an
// explicit length. Note that the variant does not take ownership of the string
// so it must stay alive as long as the variant does. Use an arena to create a
// variant that does take ownership.
pton_variant_t pton_c_str(const char *chars);

// Constructor for a binary blob. The size is in bytes. This does not copy the
// string so it has to stay alive for as long as the variant is used. Use an
// arena to create a variant that does copy the blob.
pton_variant_t pton_blob(const void *data, uint32_t size);

// Is the given value an integer?
bool pton_is_integer(pton_variant_t variant);

// Is the given value null?
bool pton_is_null(pton_variant_t variant);

// Is this value an array?
bool pton_is_array(pton_variant_t variant);

// Is this value a map?
bool pton_is_map(pton_variant_t variant);

// Is this an identity token?
bool pton_is_id(pton_variant_t variant);

// Returns the value of the given boolean if it is a boolean, otherwise false.
// In other words, true iff the value is the boolean true value.
bool pton_bool_value(pton_variant_t variant);

// Returns the value of the given boolean if it is a boolean, otherwise the
// given value.
bool pton_bool_value_with_default(pton_variant_t variant, bool if_not_bool);

// Returns the integer value of this variant if it is an integer, otherwise
// 0.
int64_t pton_int64_value(pton_variant_t variant);

// Returns the given value's type.
pton_type_t pton_type(pton_variant_t variant);

// Returns the length of the given value if it is a string, otherwise 0.
uint32_t pton_string_length(pton_variant_t variant);

// Returns the characters of this string if it is a string, otherwise NULL.
const char *pton_string_chars(pton_variant_t variant);

// Returns the backing character array of this string if it is a mutable string,
// otherwise NULL.
char *pton_string_mutable_chars(pton_variant_t variant);

// Returns the encoding of the given string if it is a string, otherwise
// PTON_CHARSET_NONE.
pton_charset_t pton_string_encoding(pton_variant_t variant);

// If this variant is a blob, returns the number of bytes. If not, returns 0.
uint32_t pton_blob_size(pton_variant_t variant);

// If this variant is a blob returns the blob data. If not returns NULL.
const void *pton_blob_data(pton_variant_t variant);

// Returns the length of this array, 0 if this is not an array.
uint32_t pton_array_length(pton_variant_t variant);

// Adds the given value at the end of the array if it is mutable. Returns
// true if adding succeeded. Adding will fail if the variant is not an array or
// not mutable.
bool pton_array_add(pton_variant_t array, pton_variant_t value);

// Returns the index'th element, null if the index is greater than the array's
// length or the variant is not an array.
pton_variant_t pton_array_get(pton_variant_t variant, uint32_t index);

// Adds an initially null value to this array, access to which is returned
// as a sink so setting the sink will cause the array value to be set.
pton_sink_t *pton_array_add_sink(pton_variant_t array);

// Returns the number of mappings in this map, if this is a map, otherwise
// 0.
uint32_t pton_map_size(pton_variant_t variant);

// Adds a mapping from the given key to the given value if this map is
// mutable. Returns true if setting succeeded.
bool pton_map_set(pton_variant_t variant, pton_variant_t key, pton_variant_t value);

// Adds a mapping from the given key to the given value if this map is
// mutable. Returns true if setting succeeded.
bool pton_map_set_sinks(pton_variant_t variant, pton_sink_t **key_out,
    pton_sink_t **value_out);

// Returns the mapping for the given key in the given map if this contains the
// key, otherwise null.
pton_variant_t pton_map_get(pton_variant_t variant, pton_variant_t key);

// Returns the mapping for the given key in the given map if this contains the
// key, otherwise the specified default value.
pton_variant_t pton_map_get_with_default(pton_variant_t variant,
    pton_variant_t key, pton_variant_t defawlt);

// Returns true if the given value is a map that contains a mapping for the
// given key, otherwise false.
bool pton_map_has(pton_variant_t variant, pton_variant_t key);

// Initialize the given iterator such that it can be used to iterate through
// the entries of the given map.
void pton_map_iter_init(pton_map_iter_t *iter, pton_variant_t map);

// Does the given iter have more entries?
bool pton_map_iter_has_next(pton_map_iter_t *iter);

// Advances the given iter to the next entry.
void pton_map_iter_advance(pton_map_iter_t *iter);

// Returns the current entry's key.
pton_variant_t pton_map_iter_current_key(const pton_map_iter_t *iter);

// Returns the current entry's value.
pton_variant_t pton_map_iter_current_value(const pton_map_iter_t *iter);

// Sets the given seed's header value. If the seed is frozen or the value is not
// a seed, nothing happens.
void pton_seed_set_header(pton_variant_t value, pton_variant_t header);

// Returns the given seed's header or, if the value isn't a seed, null.
pton_variant_t pton_seed_get_header(pton_variant_t value);

// Returns a 64-bit identity token with the given value.
pton_variant_t pton_id64(uint64_t value);

// Returns the value of a 64-bit identity token. Returns 0 if this is not a
// token or a token larger than 64 bits.
uint64_t pton_id64_value(pton_variant_t variant);

// Returns the size in bits of the given identity token or 0 if it's not a
// token.
uint32_t pton_id_size(pton_variant_t variant);

// Returns true iff the value is locally immutable. Note that even if this
// returns true it doesn't mean that nothing about this value can change -- it
// may contain references to other values that are mutable.
bool pton_is_frozen(pton_variant_t variant);

// Renders the value locally immutable. Values referenced from this one may
// be mutable so it may still change indirectly, just not this concrete
// value. This function is idempotent.
void pton_ensure_frozen(pton_variant_t);

// Creates and returns new plankton arena.
pton_arena_t *pton_new_arena();

// Frees all the resources tied to the given arena.
void pton_dispose_arena(pton_arena_t *arena);

// Returns true if this value is identical to the given value. Integers and
// strings are identical if their contents are the same, the singletons are
// identical to themselves, and structured values are identical if they were
// created by the same new_... call. So two arrays with the same values are
// not necessarily considered identical.
bool pton_variants_equal(pton_variant_t a, pton_variant_t b);

// Creates and returns a new variant string. The string is fully owned by
// the arena so the character array can be disposed after this call returns.
// The length of the string is determined using strlen.
pton_variant_t pton_new_c_str(pton_arena_t *arena, const char *str);

// Creates and returns a new variant string. The string is fully owned by
// the arena so the character array can be disposed after this call returns.
pton_variant_t pton_new_string(pton_arena_t *arena, const char *str, uint32_t length);

// Creates and returns a new mutable variant string of the given length,
// initialized to all '\0's. Note that this doesn't mean that the string is
// initially empty. Variant strings can handle null characters so what you
// get is a 'length' long string where all the characters are null. The null
// terminator is implicitly allocated in addition to the requested length, so
// you only need to worry about the non-null characters.
pton_variant_t pton_new_mutable_string(pton_arena_t *arena, uint32_t size);

// Constructor for string-valued variants with an explicitly given encoding.
pton_variant_t pton_new_string_with_encoding(pton_arena_t *arena,
    const void *chars, uint32_t length, pton_variant_t encoding);

// Creates and returns a new mutable array value.
pton_variant_t pton_new_array(pton_arena_t *arena);

// Creates and returns a new mutable array value.
pton_variant_t pton_new_array_with_capacity(pton_arena_t *arena, uint32_t init_capacity);

// Creates and returns a new mutable map value.
pton_variant_t pton_new_map(pton_arena_t *arena);

// Creates and returns a new mutable seed value.
pton_variant_t pton_new_seed(pton_arena_t *arena);

// Creates and returns a new sink value.
pton_sink_t *pton_new_sink(pton_arena_t *arena, pton_variant_t *out);

// Sets the value of this sink, if it hasn't already been set. Otherwise this
// is a no-op. Returns whether the value was set.
bool pton_sink_set(pton_sink_t *sink, pton_variant_t value);

// If this sink has not already been assigned, creates an array, stores it as
// the value of this sink, and returns it.
pton_variant_t pton_sink_as_array(pton_sink_t *sink);

// If this sink has not already been assigned, creates a map, stores it as the
// value of this sink, and returns it.
pton_variant_t pton_sink_as_map(pton_sink_t *sink);

// Creates and returns a new sink that is independent from this one but whose
// eventual value can be used to set this one. This can be useful in cases
// where you need a utility sink for some sub-computation.
pton_sink_t *pton_sink_new_sink(pton_sink_t *sink, pton_variant_t *out);

// Create a new plankton assembler.
pton_assembler_t *pton_new_assembler();

// Disposes a plankton assembler.
void pton_dispose_assembler(pton_assembler_t *assm);

// Writes an array header for an array with the given number of elements. This
// must be followed immediately by the elements.
bool pton_assembler_begin_array(pton_assembler_t *assm, uint32_t length);

// Writes a map header for a map with the given number of mappings. This must be
// followed immediately by the mappings, keys and values alternating.
bool pton_assembler_begin_map(pton_assembler_t *assm, uint32_t size);

// Writes a seed header.
bool pton_assembler_begin_seed(pton_assembler_t *assm, uint32_t headerc, uint32_t fieldc);

// Writes the given boolean value.
bool pton_assembler_emit_bool(pton_assembler_t *assm, bool value);

// Writes the null value.
bool pton_assembler_emit_null(pton_assembler_t *assm);

// Writes an int64 with the given value.
bool pton_assembler_emit_int64(pton_assembler_t *assm, int64_t value);

// Writes a blob with the given contents.
bool pton_assembler_emit_blob(pton_assembler_t *assm, const void *data, uint32_t size);

// Writes an utf8-encoded string.
bool pton_assembler_emit_default_string(pton_assembler_t *assm, const char *chars,
    uint32_t length);

// Writes the payload part of a string with an explicit encoding.
bool pton_assembler_emit_string_with_encoding(pton_assembler_t *assm,
    pton_charset_t encoding, const void *chars, uint32_t length);

// Writes an (up to) 64-bit identity token.
bool pton_assembler_emit_id64(pton_assembler_t *assm, uint32_t size,
    uint64_t value);

// Writes a reference to the previously seen value at the given offset.
bool pton_assembler_emit_reference(pton_assembler_t *assm, uint64_t offset);

// Returns the code written by the assembler. The result is still owned by the
// assembler and any further modification invalidates a previously peeked
// result. Typically you'll want to immediately copy the data away.
blob_t pton_assembler_peek_code(pton_assembler_t *assm);

// Works the same as peek code except also gives up ownership of the data so the
// assembler can be disposed without affecting the code object.
blob_t pton_assembler_release_code(pton_assembler_t *assm);

// Disposes a block of memory returned from this assembler.
void pton_assembler_dispose_code(blob_t memory);

typedef enum pton_instr_opcode_t {
  PTON_OPCODE_INT64,
  PTON_OPCODE_ID64,
  PTON_OPCODE_DEFAULT_STRING,
  PTON_OPCODE_STRING_WITH_ENCODING,
  PTON_OPCODE_BEGIN_ARRAY,
  PTON_OPCODE_BEGIN_MAP,
  PTON_OPCODE_NULL,
  PTON_OPCODE_BOOL,
  PTON_OPCODE_BEGIN_SEED,
  PTON_OPCODE_REFERENCE,
  PTON_OPCODE_BLOB
} pton_instr_opcode_t;

// Describes an individual binary plankton code instruction.
typedef struct {
  pton_instr_opcode_t opcode;
  size_t size;
  union pton_instr_payload_t {
    bool bool_value;
    int64_t int64_value;
    uint32_t array_length;
    uint32_t map_size;
    struct {
      uint32_t headerc;
      uint32_t fieldc;
    } seed_data;
    struct {
      uint32_t length;
      const uint8_t *contents;
    } default_string_data;
    struct {
      uint32_t length;
      const uint8_t *contents;
    } blob_data;
    struct {
      pton_charset_t encoding;
      uint32_t length;
      const uint8_t *contents;
    } string_with_encoding_data;
    struct {
      uint32_t size;
      uint64_t value;
    } id64;
    uint64_t reference_offset;
  } payload;
} pton_instr_t;

// Decodes the plankton instruction starting at the given code pointer and
// extending no more than the given size. The output is written into the output
// parameter. Returns true if disassembling succeeded, false if not.
bool pton_decode_next_instruction(const uint8_t *code, size_t size,
    pton_instr_t *instr_out);

// Returns true if the given input is valid plankton.
bool pton_validate(const void *code, size_t size);

// Creates and returns a new command-line reader. Dispose after use with
// pton_dispose_command_line_reader();
pton_command_line_reader_t *pton_new_command_line_reader();

// Frees up the resources allocated for the given reader.
void pton_dispose_command_line_reader(pton_command_line_reader_t *that);

// Parses the given command-line and returns a command line object representing
// the result. The result will be valid as long as the reader that returned it
// exists.
pton_command_line_t *pton_command_line_reader_parse(
    pton_command_line_reader_t *reader, int argc, const char **argv);

// Returns the number of positional arguments in the given command line.
size_t pton_command_line_argument_count(pton_command_line_t *that);

// Returns the value of the i'th positional argument in the given command line.
// If i is past the end of the command line returns null.
pton_variant_t pton_command_line_argument(pton_command_line_t *that, uint32_t i);

// Returns the number of options in the given command line.
size_t pton_command_line_option_count(pton_command_line_t *that);

// Returns true if the given command line is the result of a successful parse.
bool pton_command_line_is_valid(pton_command_line_t *that);

// If the given command line is the result of a failed parse, returns a syntax
// error value that describes the problem. Otherwise returns NULL. The returned
// value is valid as long as the reader that produced the command line.
pton_syntax_error_t *pton_command_line_error(pton_command_line_t *that);

// Returns the value of the option with the given key in the given command line.
// If the key doesn't exist returns the given default value.
pton_variant_t pton_command_line_option(pton_command_line_t *that,
    pton_variant_t key, pton_variant_t defawlt);

// Given a syntax error, returns character where the error occurred. If it was
// the end of the input \0 will be returned.
char pton_syntax_error_offender(pton_syntax_error_t *that);

// Serialize the given value onto the given assembler.
void pton_binary_writer_write(pton_assembler_t *assm, pton_variant_t value);

#endif // _PLANKTON_H
