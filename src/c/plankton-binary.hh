//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

// Definitions used by the binary plankton codec. This is all internal stuff,
// it's in a header file such that the tests can see it.

#ifndef _PLANKTON_BINARY
#define _PLANKTON_BINARY

#include "plankton-inl.hh"

namespace plankton {

// Various utilities shared between the binary reader and writer.
class BinaryImplUtils {
public:
  // Opcode values.
  enum opcode_t {
    boTrue,
    boFalse,
    boNull,
    boInteger,
    boArray
  };
};

} // plankton

#endif // _PLANKTON_BINARY
