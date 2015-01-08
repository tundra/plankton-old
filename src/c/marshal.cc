//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "stdc.h"

#include "marshal-inl.hh"
#include "plankton-inl.hh"

using namespace plankton;

void TypeRegistry::register_type(AbstractObjectType *type) {
  types_.set(type->header(), type);
}

AbstractObjectType *TypeRegistry::resolve_type(Variant header) {
  AbstractObjectType **type = types_[header];
  return (type == NULL) ? NULL : *type;
}
