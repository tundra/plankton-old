//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// Inline definitions for the C plankton api.

#ifndef _PLANKTON_INL_H
#define _PLANKTON_INL_H

#include "plankton.h"

// Initializes a variant representing an integer with the given value. Note
// that this is funky when used with a literal 0 because it also matches the
// pointer constructors.
static pton_variant_t pton_integer(int64_t value) {
  pton_variant_t result;
  result.repr_tag_ = pton_variant_t::rtInteger;
  result.payload_.as_integer_ = value;
  return result;
}

// Returns the value of the given boolean if it is a boolean, otherwise false.
// In other words, true iff the value is the boolean true value.
static bool pton_get_bool_value(pton_variant_t variant) {
  return variant.repr_tag_ == pton_variant_t::rtTrue;
}

// Constructor for string-valued variants. Note that the variant does not take
// ownership of the string so it must stay alive as long as the variant does.
// Use an arena to create a variant that does take ownership.
static pton_variant_t pton_string(const char *chars, size_t length) {
  pton_variant_t result;
  result.repr_tag_ = pton_variant_t::rtExternalString;
  result.payload_.as_external_string_.length_ = length;
  result.payload_.as_external_string_.chars_ = chars;
  return result;
}

// Constructor for string-valued variants. This function uses strlen to
// determine the length of the string; use pton_string instead to pass an
// explicit length. Note that the variant does not take ownership of the string
// so it must stay alive as long as the variant does. Use an arena to create a
// variant that does take ownership.
static pton_variant_t pton_c_str(const char *chars) {
  return pton_string(chars, strlen(chars));
}

// Constructor for a binary blob. The size is in bytes. This does not copy the
// string so it has to stay alive for as long as the variant is used. Use an
// arena to create a variant that does copy the blob.
static pton_variant_t pton_blob(const void *data, size_t size) {
  pton_variant_t result;
  result.repr_tag_ = pton_variant_t::rtExternalBlob;
  result.payload_.as_external_blob_.data_ = data;
  result.payload_.as_external_blob_.size_ = size;
  return result;
}

// Returns the integer value of this variant if it is an integer, otherwise
// 0.
static int64_t pton_get_integer_value(pton_variant_t variant) {
  return pton_is_integer(variant) ? variant.payload_.as_integer_ : 0;
}

#endif // _PLANKTON_INL_H
