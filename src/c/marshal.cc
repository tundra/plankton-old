//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "c/stdc.h"

#include "marshal-inl.hh"
#include "plankton-inl.hh"
#include "utils/log.hh"

using namespace plankton;

void TypeRegistry::register_type(AbstractSeedType *type) {
  types_.set(type->header(), type);
}

AbstractSeedType *TypeRegistry::resolve_type(Variant header) {
  AbstractSeedType **type_ref = types_[header];
  if (type_ref != NULL)
    return *type_ref;
  for (size_t i = 0; i < fallbacks_.size(); i++) {
    AbstractSeedType *type = fallbacks_[i]->resolve_type(header);
    if (type != NULL)
      return type;
  }
  return NULL;
}
