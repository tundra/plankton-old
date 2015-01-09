//- Copyright 2015 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#ifndef _VARIANT_INL_HH
#define _VARIANT_INL_HH

#include "variant.hh"

// Allocate a block of memory within the given factory.
inline void *operator new(size_t size, plankton::Factory &factory) {
  return factory.alloc_raw(size);
}

// Allocate a block of memory within the given factory.
inline void *operator new(size_t size, plankton::Factory *factory) {
  return factory->alloc_raw(size);
}

#endif // _VARIANT_INL_HH
