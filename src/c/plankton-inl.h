//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// Inline definitions for the C plankton api.

#ifndef _PLANKTON_INL_H
#define _PLANKTON_INL_H

#include "plankton.h"

// Initializes a variant representing an integer with the given value. Note
// that this is funky when used with a literal 0 because it also matches the
// pointer constructors.
static variant_p pton_new_integer(int64_t value) {
  variant_p result;
  result.repr_tag_ = variant_p::rtInteger;
  result.payload_.as_integer_ = value;
  return result;
}

#endif // _PLANKTON_INL_H
