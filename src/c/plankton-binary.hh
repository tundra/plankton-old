#ifndef _PLANKTON_BINARY
#define _PLANKTON_BINARY

#include "plankton-inl.hh"

namespace plankton {

class BinaryImplUtils {
public:
  enum opcode_t {
    boTrue,
    boFalse,
    boNull,
    boInteger
  };
};

} // plankton

#endif // _PLANKTON_BINARY
