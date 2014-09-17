//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// C bindings of the plankton serialization format.

#ifndef _PLANKTON_H
#define _PLANKTON_H

#include "stdc.h"

struct pton_arena_array_t;
struct pton_arena_blob_t;
struct pton_arena_map_t;
struct pton_arena_string_t;
struct pton_arena_t;
struct pton_arena_value_t;
struct pton_assembler_t;
struct pton_sink_t;

union pton_variant_payload_t {
  int64_t as_integer_;
  struct {
    uint32_t length_;
    const char *chars_;
  } as_external_string_;
  struct {
    uint32_t size_;
    const void *data_;
  } as_external_blob_;
  pton_arena_value_t *as_arena_value_;
  pton_arena_array_t *as_arena_array_;
  pton_arena_map_t *as_arena_map_;
  pton_arena_string_t *as_arena_string_;
  pton_arena_blob_t *as_arena_blob_;
};

struct pton_variant_t {
  // Tags that identify the internal representation of variants.
  enum repr_tag_t {
    rtInteger = 0x10,
    rtExternalString = 0x20,
    rtArenaString = 0x21,
    rtExternalBlob = 0x30,
    rtArenaBlob = 0x31,
    rtNull = 0x40,
    rtTrue = 0x50,
    rtFalse = 0x51,
    rtArenaArray = 0x60,
    rtArenaMap = 0x70
  };

  // The different types of variants. The values are the corresponding
  // representation tags downshifted by 4.
  enum type_t {
    vtInteger = 0x01,
    vtString = 0x02,
    vtBlob = 0x03,
    vtNull = 0x04,
    vtBool = 0x05,
    vtArray = 0x06,
    vtMap = 0x07
  };

  // This variant's tag.
  repr_tag_t repr_tag_;
  pton_variant_payload_t payload_;
};

// Is the given value an integer?
static bool pton_is_integer(pton_variant_t variant) {
  return variant.repr_tag_ == pton_variant_t::rtInteger;
}

// Is this value an array?
static bool pton_is_array(pton_variant_t variant) {
  return variant.repr_tag_ == pton_variant_t::rtArenaArray;
}

// Is this value a map?
static bool pton_is_map(pton_variant_t variant) {
  return variant.repr_tag_ == pton_variant_t::rtArenaMap;
}

// Returns a variant representing null.
static pton_variant_t pton_null() {
  pton_variant_t result;
  result.repr_tag_ = pton_variant_t::rtNull;
  result.payload_.as_integer_ = 0;
  return result;
}

// Returns a variant representing the boolean true.
static pton_variant_t pton_true() {
  pton_variant_t result;
  result.repr_tag_ = pton_variant_t::rtTrue;
  result.payload_.as_integer_ = 0;
  return result;
}

// Returns a variant representing the boolean false.
static pton_variant_t pton_false() {
  pton_variant_t result;
  result.repr_tag_ = pton_variant_t::rtFalse;
  result.payload_.as_integer_ = 0;
  return result;
}

// Returns a variant representing the given boolean.
static pton_variant_t pton_bool(bool value) {
  pton_variant_t result;
  result.repr_tag_ = value ? pton_variant_t::rtTrue : pton_variant_t::rtFalse;
  result.payload_.as_integer_ = 0;
  return result;
}

// Returns the given value's type.
pton_variant_t::type_t pton_get_type(pton_variant_t variant);

// Returns the length of the given value if it is a string, otherwise 0.
uint32_t pton_get_string_length(pton_variant_t variant);

// Returns the characters of this string if it is a string, otherwise NULL.
const char *pton_get_string_chars(pton_variant_t variant);

// If this variant is a blob, returns the number of bytes. If not, returns 0.
uint32_t pton_get_blob_size(pton_variant_t variant);

// If this variant is a blob returns the blob data. If not returns NULL.
const void *pton_get_blob_data(pton_variant_t variant);

// Returns the length of this array, 0 if this is not an array.
uint32_t pton_get_array_length(pton_variant_t variant);

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

pton_assembler_t *pton_new_assembler();

void pton_dispose_assembler(pton_assembler_t *assm);

bool pton_assembler_begin_array(pton_assembler_t *assm, uint32_t length);

bool pton_assembler_emit_bool(pton_assembler_t *assm, bool value);

bool pton_assembler_emit_null(pton_assembler_t *assm);

bool pton_assembler_emit_int64(pton_assembler_t *assm, int64_t value);

bool pton_assembler_flush(pton_assembler_t *assm, uint8_t **memory_out, size_t *size_out);

#endif // _PLANKTON_H
