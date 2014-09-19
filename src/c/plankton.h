//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// C bindings of the plankton serialization format.

#ifndef _PLANKTON_H
#define _PLANKTON_H

#include "stdc.h"

typedef struct pton_arena_array_t pton_arena_array_t;
typedef struct pton_arena_blob_t pton_arena_blob_t;
typedef struct pton_arena_map_t pton_arena_map_t;
typedef struct pton_arena_string_t pton_arena_string_t;
typedef struct pton_arena_t pton_arena_t;
typedef struct pton_arena_value_t pton_arena_value_t;
typedef struct pton_assembler_t pton_assembler_t;
typedef struct pton_sink_t pton_sink_t;

// The different types of variants. The values are the corresponding
// representation tags downshifted by 4.
typedef enum pton_type_t {
  PTON_INTEGER = 0x01,
  PTON_STRING = 0x02,
  PTON_BLOB = 0x03,
  PTON_NULL = 0x04,
  PTON_BOOL = 0x05,
  PTON_ARRAY = 0x06,
  PTON_MAP = 0x07
} pton_type_t;

// A value that encodes a value and the value's type. It provides a uniform
// interface that abstracts over all the different types that can be encoded
// to and decoded from plankton.
typedef struct {
  // The header of the variant. This part is the same in all variants.
  struct header_t {
    // The tag that identifies what kind of variant we're dealing with.
    enum repr_tag_t {
        REPR_INT64 = 0x10,
        REPR_EXTN_STRING = 0x20,
        REPR_ARNA_STRING = 0x21,
        REPR_EXTN_BLOB = 0x30,
        REPR_ARNA_BLOB = 0x31,
        REPR_NULL = 0x40,
        REPR_TRUE = 0x50,
        REPR_FALSE = 0x51,
        REPR_ARNA_ARRAY = 0x60,
        REPR_ARNA_MAP = 0x70
    } repr_tag_ IF_MSVC(, : 8);
    // A tag used to identify the version of plankton that produced this value.
    // All variants returned from a binary plankton implementation will have the
    // same version tag, and the version of all arguments will be checked
    // against that tag. If a function is called with the wrong version the call
    // will abort immediately. This can be used to trap binary incompatibility
    // by bumping the version when making incompatible changes to variant
    // representation. It is also useful in trapping invalid or uninitialized
    // data viewed as variants.
    uint8_t binary_version_ IF_MSVC(, : 8);
    // A length or size field. Only used by some variants but it wastes space
    // on 64 bits to put it in the payload.
    uint32_t length_ IF_MSVC(, : 32);
  } header_;

  // The contents of a variant. This is the part that changes depending on the
  // type of the variant.
  union payload_t {
    int64_t as_int64_;
    pton_arena_value_t *as_arena_value_;
    pton_arena_array_t *as_arena_array_;
    pton_arena_map_t *as_arena_map_;
    pton_arena_string_t *as_arena_string_;
    pton_arena_blob_t *as_arena_blob_;
    const void *as_external_blob_data_;
    const char *as_external_string_chars_;
  } payload_;

} pton_variant_t;

// Returns a variant representing null.
pton_variant_t pton_null();

// Returns a variant representing the boolean true.
pton_variant_t pton_true();

// Returns a variant representing the boolean false.
pton_variant_t pton_false();

// Returns a variant representing the given boolean.
pton_variant_t pton_bool(bool value);

// Initializes a variant representing an integer with the given value.
pton_variant_t pton_integer(int64_t value);

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

// Is this value an array?
bool pton_is_array(pton_variant_t variant);

// Is this value a map?
bool pton_is_map(pton_variant_t variant);

// Returns the value of the given boolean if it is a boolean, otherwise false.
// In other words, true iff the value is the boolean true value.
bool pton_bool_value(pton_variant_t variant);

// Returns the integer value of this variant if it is an integer, otherwise
// 0.
int64_t pton_int64_value(pton_variant_t variant);

// Returns the given value's type.
pton_type_t pton_type(pton_variant_t variant);

// Returns the length of the given value if it is a string, otherwise 0.
uint32_t pton_string_length(pton_variant_t variant);

// Returns the characters of this string if it is a string, otherwise NULL.
const char *pton_string_chars(pton_variant_t variant);

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

// Returns the number of mappings in this map, if this is a map, otherwise
// 0.
uint32_t pton_map_size(pton_variant_t variant);

// Adds a mapping from the given key to the given value if this map is
// mutable. Returns true if setting succeeded.
bool pton_map_set(pton_variant_t variant, pton_variant_t key, pton_variant_t value);

// Returns the mapping for the given key in the given map if this contains the
// key, otherwise null.
pton_variant_t pton_map_get(pton_variant_t variant, pton_variant_t key);

// Returns true iff the value is locally immutable. Note that even if this
// returns true it doesn't mean that nothing about this value can change -- it
// may contain references to other values that are mutable.
bool pton_is_frozen(pton_variant_t variant);

// Renders the value locally immutable. Values referenced from this one may
// be mutable so it may still change indirectly, just not this concrete
// object. This function is idempotent.
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

// Creates and returns a new mutable array value.
pton_variant_t pton_new_array(pton_arena_t *arena);

// Creates and returns a new mutable array value.
pton_variant_t pton_new_array_with_capacity(pton_arena_t *arena, uint32_t init_capacity);

// Creates and returns a new mutable map value.
pton_variant_t pton_new_map(pton_arena_t *arena);

// Creates and returns a new sink value.
pton_sink_t *pton_new_sink(pton_arena_t *arena);

// Sets the value of this sink, if it hasn't already been set. Otherwise this
// is a no-op. Returns whether the value was set.
bool pton_sink_set(pton_sink_t *sink, pton_variant_t value);

// Returns the value stored in this sink. If the sink is empty the result is
// null.
pton_variant_t pton_sink_get(pton_sink_t *sink);

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

// Writes an object header.
bool pton_assembler_begin_object(pton_assembler_t *assm);

// Writes the header of an environment reference.
bool pton_assembler_begin_environment_reference(pton_assembler_t *assm);

// Writes the given boolean value.
bool pton_assembler_emit_bool(pton_assembler_t *assm, bool value);

// Writes the null value.
bool pton_assembler_emit_null(pton_assembler_t *assm);

// Writes an int64 with the given value.
bool pton_assembler_emit_int64(pton_assembler_t *assm, int64_t value);

// Writes an utf8-encoded string.
bool pton_assembler_emit_utf8(pton_assembler_t *assm, const char *chars,
    uint32_t length);

// Writes a reference to the previously seen object at the given offset.
bool pton_assembler_emit_reference(pton_assembler_t *assm, uint64_t offset);

// Flushes the given assembler, writing the output into the given parameters.
// The caller assumes ownership of the returned array and is responsible for
// freeing it. This doesn't free the assembler, it must still be disposed with
// pton_dispose_assembler.
bool pton_assembler_flush(pton_assembler_t *assm, uint8_t **memory_out, size_t *size_out);

#endif // _PLANKTON_H
