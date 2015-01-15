//- Copyright 2015 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#ifndef _MARSHAL_INL_HH
#define _MARSHAL_INL_HH

#include "marshal.hh"

namespace plankton {

template <typename T>
inline T *Variant::native_as(ConcreteSeedType<T> *type) {
  return type->cast(native_type(), native_object());
}

template <typename T>
T *ConcreteSeedType<T>::cast(AbstractSeedType *type, void *object) {
  return (this == type) ? static_cast<T*>(object) : NULL;
}

template <typename T>
Variant SeedType<T>::get_initial_instance(Variant header, Factory *arena) {
  T *instance = (create_)(header, arena);
  return arena->new_native(instance, this);
}

template <typename T>
Variant SeedType<T>::get_complete_instance(Variant initial, Variant payload,
    Factory *factory) {
  T *value = initial.native_as(this);
  if (value == NULL || complete_.is_empty())
    // It's unclear how or if this can happen but just in case better handle it
    // specially.
    return initial;
  (complete_)(value, payload, factory);
  return initial;
}

template <typename T>
Variant SeedType<T>::encode_instance(Native wrapped, Factory *factory) {
  T *value = wrapped.as(this);
  if (value == NULL || encode_.is_empty())
    return Variant::null();
  return (encode_)(value, factory);
}

template <typename T>
SeedType<T>::SeedType(Variant header, new_instance_t create,
    complete_instance_t complete, encode_instance_t encode)
  : header_(header)
  , create_(create)
  , complete_(complete)
  , encode_(encode) { }

template <typename T>
void VariantMap<T>::set(Variant key, const T &value) {
  if (key.is_string()) {
    strings_[key] = value;
  } else {
    set_generic(key, value);
  }
}

template <typename T>
void VariantMap<T>::set_generic(Variant key, const T &value) {
  T *existing = get_generic(key);
  if (existing == NULL) {
    GenericMapping mapping = {key, value};
    generic_.push_back(mapping);
  } else {
    *existing = value;
  }
}

template <typename T>
T *VariantMap<T>::operator[](Variant key) {
  if (key.is_string()) {
    typename StringMap::iterator i = strings_.find(key);
    return (i == strings_.end()) ? NULL : &i->second;
  } else {
    return get_generic(key);
  }
}

template <typename T>
T *VariantMap<T>::get_generic(Variant key) {
  for (typename GenericVector::iterator i = generic_.begin();
       i != generic_.end();
       i++) {
    if (i->key == key)
      return &i->value;
  }
  return NULL;
}

template <typename T>
size_t VariantMap<T>::StringHasher::operator()(const String &key) const {
  platform_hash<const char*> hasher;
  return hasher(key.string_chars());
}

} // namespace plankton

#endif // _MARSHAL_INL_HH
