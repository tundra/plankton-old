//- Copyright 2015 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#ifndef _VARIANT_INL_HH
#define _VARIANT_INL_HH

#include "variant.hh"

namespace plankton {

template <typename T>
inline T *Variant::native_as(ConcreteObjectType<T> *type) {
  return type->cast(native_type(), native_value());
}

template <typename T>
T *ConcreteObjectType<T>::cast(AbstractObjectType *type, void *object) {
  return (this == type) ? static_cast<T*>(object) : NULL;
}

template <typename T>
Variant ObjectType<T>::get_initial_instance(Variant header, Factory *arena) {
  T *instance = (create_)(header, arena);
  return arena->new_native_object(this, instance);
}

template <typename T>
Variant ObjectType<T>::get_complete_instance(Variant initial, Variant payload,
    Factory *factory) {
  T *value = initial.native_as(this);
  if (value == NULL)
    // It's unclear how or if this can happen but just in case better handle it
    // specially.
    return initial;
  (complete_)(value, payload, factory);
  return initial;
}

template <typename T>
ObjectType<T>::ObjectType(Variant header, new_instance_t create, complete_instance_t complete)
  : header_(header)
  , create_(create)
  , complete_(complete) { }

} // namespace plankton

// Allocate a block of memory within the given factory.
inline void *operator new(size_t size, plankton::Factory &factory) {
  return factory.alloc_raw(size);
}

#endif // _PLANKTON_HH
