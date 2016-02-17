//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// Support for native C/C++ object marshalling.

#ifndef _MARSHAL_HH
#define _MARSHAL_HH

#include "c/stdc.h"

#include "c/stdhashmap.hh"
#include "utils/callback.hh"
#include "variant.hh"

namespace plankton {

// A seed type handles the process of growing a custom object in place of a
// seed. Typically you won't implement this directly but one of the two
// subtypes, SeedType and AtomicSeedType. The plain version does construction in
// two steps: creates an empty instance in the first step after just the header
// has been read and then after the payload has been read sets the instance's
// contents. The atomic version is created in a single step after both the
// header and payload have been seen. Atomic objects cannot contain references
// to themselves or appear in cycles.
class AbstractSeedType {
public:
  virtual ~AbstractSeedType() { }

  // Called immediately after the seed header has been read. The value
  // returned here will be used as the value to be referenced while reading the
  // rest of the seed.
  virtual Variant get_initial_instance(Variant header, Factory *factory) = 0;

  // Initializes the initial instance and/or returns a new instance. This is
  // called after the entire payload of the seed has been read. The value
  // returned here will be used as the value to be referenced from the end of
  // the object on. Usually, if a nontrivial value was returned as the initial
  // instance that is the value you want to return here too.
  virtual Variant get_complete_instance(Variant initial, Variant payload, Factory *factory) = 0;

  // Given a native value, returns the value to use as a replacement.
  virtual Variant encode_instance(Native value, Factory *factory) = 0;

  // Returns the header value that identifies instance of this type.
  virtual Variant header() = 0;
};

// A concrete seed type binds the type of instances and contains the common
// functionality between SeedType and AtomicSeedType.
template <typename T>
class ConcreteSeedType : public AbstractSeedType {
public:
  // Given an object pointer and its type, returns the type viewed as the type
  // represented by this type object. If the object doesn't have this type
  // NULL is returned.
  inline T *cast(AbstractSeedType *type, void *object);
};

// An seed type describes a type that can be constructed in two steps: first
// grown and then completed. The type implements this using two callbacks:
// one for construction and one for completion.
template <typename T>
class SeedType : public ConcreteSeedType<T> {
public:
  typedef tclib::callback_t<T*(Variant, Factory*)> new_instance_t;
  typedef tclib::callback_t<void(T*, Seed, Factory*)> complete_instance_t;
  typedef tclib::callback_t<Variant(T*, Factory*)> encode_instance_t;

  // Constructs an object type for plankton objects that have the given value
  // as header. Instances will be constructed using new_instance and completed
  // using complete_instance.
  SeedType(Variant header,
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

template <typename T>
class DefaultSeedType : public SeedType<T> {
public:
  DefaultSeedType(Variant header,
      typename SeedType<T>::new_instance_t new_instance = tclib::new_callback(T::new_instance),
      typename SeedType<T>::complete_instance_t complete_instance = tclib::new_callback(&T::init),
      typename SeedType<T>::encode_instance_t encode = tclib::new_callback(&T::to_seed))
    : SeedType<T>(header, new_instance, complete_instance, encode) { }
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
    // MSVC hash map stuff.
    static const size_t bucket_size = 4;
    bool operator()(const String &a, const String &b);
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

// A registry that can resolve object types during parsing based on the seed's
// headers.
class AbstractTypeRegistry {
public:
  virtual ~AbstractTypeRegistry() { }

  // Returns the type corresponding to the given header. If no type is known
  // NULL is returned.
  virtual AbstractSeedType *resolve_type(Variant header) = 0;
};

// A simple registry based on a mapping from headers to types.
class TypeRegistry : public AbstractTypeRegistry {
public:
  // Adds the given type as the mapping for its header to this registry.
  void register_type(AbstractSeedType *type);

  // Adds another type registry that will be called to resolve any types that
  // this registry itself doesn't know about. If multiple fallbacks are added
  // they will be called in the order they were added and the first non-null
  // type returned will be the result.
  void add_fallback(TypeRegistry *fallback);

  virtual AbstractSeedType *resolve_type(Variant header);
private:
  VariantMap<AbstractSeedType*> types_;
  std::vector<TypeRegistry*> fallbacks_;
};

} // namespace plankton

#endif // _MARSHAL_HH
