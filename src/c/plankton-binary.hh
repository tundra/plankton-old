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
    boInteger = 0,
    boDefaultString = 1,
    boArray = 2,
    boMap = 3,
    boNull = 4,
    boTrue = 5,
    boFalse = 6,
    boObject = 7,
    boReference = 8,
    boBeginEnvironmentReference = 9,
    boBeginStringWithEncoding = 10,
    boId = 11
  };
};

} // plankton

#endif // _PLANKTON_BINARY
