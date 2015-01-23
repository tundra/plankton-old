//- Copyright 2015 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#ifndef _MARSHAL_INL_HH
#define _MARSHAL_INL_HH

#include "marshal.hh"

namespace plankton {

// Shorthand for getting wrapped data out of a native variant. If the variant
// is a native of the given type returns the wrapped value, otherwise returns
// NULL.
template <typename T>
static inline T *native_cast(Variant variant,
    ConcreteSeedType<T> *type = default_seed_type<T>::get()) {
  return variant.native_as(type);
}

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
  // The msvc hasher uses the address to hash char pointers. The linux one on
  // the other hand doesn't appear to know how to hash a std::string.
  platform_hash<IF_MSVC(std::string, const char*)> hasher;
  // Do we need to worry about embedded nulls? In theory strings with embedded
  // nulls should get the same hash which is within the contract of a hash
  // function so it should be okay.
  return hasher(key.chars());
}

template <typename T>
bool VariantMap<T>::StringHasher::operator()(const String &a, const String &b) {
  size_t a_length = a.length();
  size_t b_length = b.length();
  if (a_length != b_length || (a_length == 0))
    // Different lengths or empty strings can be decided based just on length.
    return a_length < b_length;
  return strncmp(a.chars(), b.chars(), a_length) < 0;
}

} // namespace plankton

#endif // _MARSHAL_INL_HH
