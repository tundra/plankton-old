//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// Support for native C/C++ object marshalling.

#ifndef _MARSHAL_HH
#define _MARSHAL_HH

#include "stdc.h"

#include "callback.hh"
#include "variant.hh"

#include "std/stdhashmap.hh"

// TODO: move this into tclib and add something similar for windows.
#include <hash_fun.h>
#define platform_hash __gnu_cxx::hash

namespace plankton {

// An object type handles the process of constructing a custom object in place
// of a plankton object. Typically you won't implement this directly but one of
// the two subtypes, ObjectType and AtomicObjectType. The plain version does
// construction in two steps: creates an empty instance in the first step after
// just the header has been read and then after the payload has been read sets
// the instance's contents. The atomic version is created in a single step after
// both the header and payload have been seen. Atomic object cannot contain
// references to themselves or appear in cycles.
class AbstractObjectType {
public:
  virtual ~AbstractObjectType() { }

  // Called immediately after the object header has been read. The value
  // returned here will be used as the value to be referenced while reading the
  // rest of the object.
  virtual Variant get_initial_instance(Variant header, Factory *factory) = 0;

  // Initializes the initial instance and/or returns a new instance. This is
  // called after the entire payload of the object has been read. The value
  // returned here will be used as the value to be referenced from the end of
  // the object on. Usually, if a nontrivial value was returned as the initial
  // instance that is the value you want to return here too.
  virtual Variant get_complete_instance(Variant initial, Variant payload, Factory *factory) = 0;

  // Given a native value, returns the value to use as a replacement.
  virtual Variant encode_instance(Native value, Factory *factory) = 0;

  // Returns the header value that identifies instance of this type.
  virtual Variant header() = 0;
};

// A concrete object type binds the type of instances and contains the common
// functionality between ObjectType and AtomicObjectType.
template <typename T>
class ConcreteObjectType : public AbstractObjectType {
public:
  // Given an object pointer and its type, returns the type viewed as the type
  // represented by this type object. If the object doesn't have this type
  // NULL is returned.
  inline T *cast(AbstractObjectType *type, void *object);
};

// An object type describes a type that can be constructed in two steps: first
// created and then completed. The type implements this using two callbacks:
// one for construction and one for completion.
template <typename T>
class ObjectType : public ConcreteObjectType<T> {
public:
  typedef tclib::callback_t<T*(Variant, Factory*)> new_instance_t;
  typedef tclib::callback_t<void(T*, Object, Factory*)> complete_instance_t;
  typedef tclib::callback_t<Variant(T*, Factory*)> encode_instance_t;

  // Constructs an object type for plankton objects that have the given value
  // as header. Instances will be constructed using new_instance and completed
  // using complete_instance.
  ObjectType(Variant header,
      new_instance_t new_instance = tclib::empty_callback(),
      complete_instance_t complete_instance = tclib::empty_callback(),
      encode_instance_t encode = tclib::empty_callback());

  virtual Variant get_initial_instance(Variant header, Factory *arena);
  virtual Variant get_complete_instance(Variant initial, Variant payload, Factory *arena);
  virtual Variant encode_instance(Native value, Factory *factory);
  virtual Variant header() { return header_; }

private:
  Variant header_;
  new_instance_t create_;
  complete_instance_t complete_;
  encode_instance_t encode_;
};

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

#endif // _MARSHAL_HH
