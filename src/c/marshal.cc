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
  AbstractSeedType **type = types_[header];
  if (type != NULL)
    return *type;
  return (parent_ == NULL) ? NULL : parent_->resolve_type(header);
}
