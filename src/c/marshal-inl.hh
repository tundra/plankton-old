//- Copyright 2015 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#ifndef _MARSHAL_INL_HH
#define _MARSHAL_INL_HH

#include "marshal.hh"
#include "std/stdhashmap.hh"

// TODO: move this into tclib and add something similar for windows.
#include <hash_fun.h>
#define platform_hash __gnu_cxx::hash

namespace plankton {

template <typename T>
inline T *Variant::native_as(ConcreteObjectType<T> *type) {
  return type->cast(native_type(), native_object());
}

template <typename T>
T *ConcreteObjectType<T>::cast(AbstractObjectType *type, void *object) {
  return (this == type) ? static_cast<T*>(object) : NULL;
}

template <typename T>
Variant ObjectType<T>::get_initial_instance(Variant header, Factory *arena) {
  T *instance = (create_)(header, arena);
  return arena->new_native(instance, this);
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
Variant ObjectType<T>::encode_instance(Native wrapped, Factory *factory) {
  T *value = wrapped.as(this);
  if (value == NULL || encode_.is_empty())
    return Variant::null();
  return (encode_)(value, factory);
}

template <typename T>
ObjectType<T>::ObjectType(Variant header, new_instance_t create,
    complete_instance_t complete, encode_instance_t encode)
  : header_(header)
  , create_(create)
  , complete_(complete)
  , encode_(encode) { }

// A mapping from variants to values. This is different from a variant map in
// that the values can be of any type. A variant map also does not keep track
// of insertion order.
template <typename T>
class VariantMap {
public:
  // Maps the given key to the given value. If there is already a mapping it is
  // replaced by this one. The map does not take ownership of the key, it is
  // up to the caller to ensure that it's valid as long as the map exists.
  void set(Variant key, const T &value);

  // Returns the binding for the given key, if there is one, otherwise NULL.
  // If the map is subsequently modified the pointer is no longer guaranteed
  // to be valid.
  T *operator[](Variant key);

private:
  // Controls how strings are hashed in the string map.
  struct StringHasher {
  public:
    size_t operator()(const String &key) const;
  };

  // A non special case mapping.
  struct GenericMapping {
    Variant key;
    T value;
  };

  typedef platform_hash_map<String, T, StringHasher> StringMap;
  typedef std::vector<GenericMapping> GenericVector;

  // Looks up a binding in the generic mappings.
  T *get_generic(Variant key);

  // Add a mapping to the generic mappings.
  void set_generic(Variant key, const T &value);

  // All string mappings are stored here, for more efficient access.
  StringMap strings_;

  // Mappings that don't belong anywhere else.
  GenericVector generic_;
};

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

// A registry that can resolve object types during parsing based on the objects'
// headers.
class AbstractTypeRegistry {
public:
  virtual ~AbstractTypeRegistry() { }

  // Returns the type corresponding to the given header. If no type is known
  // NULL is returned.
  virtual AbstractObjectType *resolve_type(Variant header) = 0;
};

// A simple registry based on a mapping from headers to types.
class TypeRegistry : public AbstractTypeRegistry {
public:
  // Adds the given type as the mapping for its header to this registry.
  void register_type(AbstractObjectType *type);

  virtual AbstractObjectType *resolve_type(Variant header);
private:
  VariantMap<AbstractObjectType*> types_;
};

} // namespace plankton

#endif // _MARSHAL_INL_HH
