//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// C++ implementation of the plankton serialization format.

#ifndef _VARIANT_HH
#define _VARIANT_HH

#include "stdc.h"

BEGIN_C_INCLUDES
#include "plankton.h"
#include "utils/alloc.h"
END_C_INCLUDES

#include "callback.hh"
#include "refcount.hh"
#include "std/stdvector.hh"

// This is really just an opaque name for c api arenas but it does need a tiny
// bit of functionality just to be on the safe side wrt. destruction.
struct pton_arena_t {
public:
  virtual ~pton_arena_t() { }
};

namespace plankton {

class Variant;
class Sink;
class disposable_t;
class Map_Iterator;
class AbstractSeedType;
template <typename T> class ConcreteSeedType;

} // namespace plankton

// This provides a mechanism for registering a default type value for a given
// type. You don't have to use it but sometimes it makes things less verbose.
// If the type has a static seed_type() method it will automatically be used
// to provide the default type.
template <typename T> struct default_seed_type {
  // If you're getting an error here that type T doesn't have a seed_type()
  // member then it is likely because code somewhere is trying to create a
  // native plankton object for that type in a way that expects a default type
  // descriptor to be available, but there isn't one. The way to fix this is
  // to either add a static create_type method, add an explicit specialization
  // of default_seed_type for the type, or pass a type explicitly in the code
  // that is trying to create a plankton::Native.
  static plankton::ConcreteSeedType<T> *get() { return T::seed_type(); }
};

namespace plankton {

// A plankton variant. A variant can represent any of the plankton data types.
// Some variant values, like integers and external strings, can be constructed
// without allocation whereas others, like arrays and maps, must be allocated in
// an arena. Some variant types can be mutable, such as strings and arrays, to
// allow values to be built incrementally. All variant types can be frozen such
// that any further modification will be rejected.
//
// Variants can be handled in two equivalent but slightly different ways,
// depending on what's convenient. The basic Variant type has methods for
// interacting with all the different types. For instance you can ask for the
// array length of any variant by calling Variant::array_length, regardless of
// whether you're statically sure it's an array. For arrays you'll get the
// actual length back, for any other type there's a fallback result which in
// this case is 0.
//
// Alternatively there are specialized types such as Array that provide the
// same functionality but in a more convenient form. So instead of calling
// Variant::array_length you can convert the value to an Array and use
// Array::length. Semantically this is equivalent but it makes your
// assumptions clear and the code more concise.
class Variant {
public:
  typedef pton_variant_t::pton_variant_header_t header_t;

  // Initializes a variant representing null.
  inline Variant() : value_(pton_null()) { }

  // Converts a C-style variant to a C++-style one.
  inline Variant(pton_variant_t value) : value_(value) { }

  // Static method that returns a variant representing null. Equivalent to
  // the no-arg constructor but more explicit.
  static inline Variant null() { return Variant(pton_null()); }

  // Returns a variant representing the boolean true. Called 'yes' because
  // 'true' is a keyword.
  static inline Variant yes() { return Variant(pton_true()); }

  // Returns a variant representing the boolean false. Called 'no' because
  // 'false' is a keyword.
  static inline Variant no() { return Variant(pton_false()); }

  // Returns a variant representing a bool, false if the value is 0, true
  // otherwise. Called 'boolean' because 'bool' has a tendency to be taken.
  static inline Variant boolean(int value) { return Variant(pton_bool(value)); }

  // Returns a variant representing a 64-bit identity token.
  static inline Variant id64(uint64_t value) { return Variant(pton_id64(value)); }

  // Returns a variant representing a 32-bit identity token.
  static inline Variant id32(uint32_t value) { return Variant(pton_id32(value)); }

  // Returns a variant representing a 32-bit identity token.
  static inline Variant id(uint32_t size, uint64_t value) { return Variant(pton_id(size, value)); }

  // Initializes a variant representing an integer with the given value. Note
  // that this is funky when used with a literal 0 because it also matches the
  // pointer constructors.
  inline Variant(int64_t integer);

  // Static constructor for integer variants that doesn't rely on overloading,
  // unlike the constructor.
  static inline Variant integer(int64_t value);

  // Initializes a variant representing a string with the given contents, using
  // strlen to determine the string's length. This does not copy the string so
  // it has to stay alive for as long as the variant is used.
  inline Variant(const char *string);

  // Explicit constructor for string-valued variants. Note that the variant does
  // not take ownership of the string so it must stay alive as long as the
  // variant does. Use an arena to create a variant that does take ownership.
  static inline Variant string(const char *string) { return pton_c_str(string); }

  // Initializes a variant representing a string with the given contents. This
  // does not copy the string so it has to stay alive for as long as the
  // variant is used. Use an arena to create a variant that does copy the string.
  inline Variant(const char *string, uint32_t length);

  // Explicit constructor for string-valued variants. Note that the variant does
  // not take ownership of the string so it must stay alive as long as the
  // variant does. Use an arena to create a variant that does take ownership.
  static inline Variant string(const char *string, uint32_t length);

  // Explicit constructor for a binary blob. The size is in bytes. This
  // does not copy the string so it has to stay alive for as long as the
  // variant is used. Use an arena to create a variant that does copy the string.
  static Variant blob(const void *data, uint32_t size);

  // Returns this value's type.
  pton_type_t type() const;

  // Returns the integer value of this variant if it is an integer, otherwise
  // 0.
  inline int64_t integer_value() const;

  // Returns the length of this string if it is a string, otherwise 0.
  uint32_t string_length() const;

  // Returns the characters of this string if it is a string, otherwise NULL.
  const char *string_chars() const;

  char *string_mutable_chars() const;

  // Returns this string's encoding if this is a string, otherwise null.
  pton_charset_t string_encoding() const;

  // If this variant is a blob, returns the number of bytes. If not, returns 0.
  uint32_t blob_size() const;

  // If this variant is a blob returns the blob data. If not returns NULL.
  const void *blob_data() const;

  // If this variant is a mutable blob returns the blob data. If not returns
  // NULL.
  void *blob_mutable_data();

  // Returns the length of this array, 0 if this is not an array.
  uint32_t array_length() const;

  // Returns the index'th element, null if the index is greater than the array's
  // length.
  Variant array_get(uint32_t index) const;

  // Adds the given value at the end of this array if it is mutable. Returns
  // true if adding succeeded.
  bool array_add(Variant value);

  // Adds an initially null value to this array, access to which is returned
  // as a sink so setting the sink will cause the array value to be set.
  Sink array_add_sink();

  // Returns this native variant viewed under the given type, but only if this
  // is a native that has that type. If not, NULL is returned.
  template <typename T>
  inline T *native_as(ConcreteSeedType<T> *type);

  // Returns the type of this native object, or NULL if this is not native.
  AbstractSeedType *native_type();

  // Returns a raw pointer to the native object. The only really safe way to
  // get access to the value under its type is using ::as so be careful with
  // this one. If this is not a native return NULL.
  void *native_object();

  // Returns the number of mappings in this map, if this is a map, otherwise
  // 0.
  uint32_t map_size() const;

  // Adds a mapping from the given key to the given value if this map is
  // mutable. Returns true if setting succeeded.
  bool map_set(Variant key, Variant value);

  bool map_set(Sink *key_out, Sink *value_out);

  // Returns the mapping for the given key in this map if this contains the
  // key, otherwise null.
  Variant map_get(Variant key) const;

  // Returns an iterator for iterating this map, if this is a map, otherwise an
  // empty iterator. The first call to advance will yield the first mapping, if
  // there is one.
  Map_Iterator map_begin() const;

  Map_Iterator map_end() const;

  // Returns the header of this seed, if this is a seed, otherwise null.
  Variant seed_header() const;

  // Sets the header of this seed if this is a mutable seed. Returns true if
  // setting succeeded, otherwise false.
  bool seed_set_header(Variant value);

  // Sets the value of a field if this is a mutable seed. Returns true if
  // setting succeeded, otherwise false.
  bool seed_set_field(Variant key, Variant value);

  // If this is a seed with a field with the given key, returns the value of
  // that field. Otherwise returns the null value.
  Variant seed_get_field(Variant key);

  // If this is a seed, returns the number of fields it contains. If not returns
  // 0.
  uint32_t seed_field_count();

  // Returns an iterator for traversing the fields of this seed.
  Map_Iterator seed_fields_begin();

  // Returns an iterator that indicates the limit when traversing the fields
  // of this seed.
  Map_Iterator seed_fields_end();

  // Returns the size in bits of this id value or 0 if this is not an id.
  uint32_t id_size() const;

  // Returns the value of a 64-bit id or 0 if this is not an id of at most 64
  // bits.
  uint64_t id64_value() const;

  // Returns the value of this boolean if it is a boolean, otherwise false. In
  // other words, true iff this is the boolean true value. Note that this is
  // different from the bool() operator which returns true for all values except
  // null. Think of this as an accessor for the value of something you know is
  // a boolean, whereas the bool() operator tests whether the variant is a
  // nontrivial value.
  inline bool bool_value() const;

  // Returns true if this is a truthy value, that is, not the null value. This
  // is useful in various conversion which return a truthy value on success and
  // null on failure. Note that this is different from the bool_value() function
  // which returns true only for the true value. Think of this as a test of
  // whether the variant is a nontrivial value, similar to testing whether a
  // pointer is NULL, whereas bool_value is an accessor for the value of
  // something you know is a boolean.
  inline operator bool() const;

  // Returns true if this value is identical to the given value. Integers and
  // strings are identical if their contents are the same, the singletons are
  // identical to themselves, and structured values are identical if they were
  // created by the same new_... call. So two arrays with the same values are
  // not necessarily considered identical.
  bool operator==(Variant that);

  // Returns true iff this value is locally immutable. Note that even if this
  // returns true it doesn't mean that nothing about this value can change -- it
  // may contain references to other values that are mutable.
  bool is_frozen() const;

  // Renders this value locally immutable. Values referenced from this one may
  // be mutable so it may still change indirectly, just not this concrete
  // object.
  void ensure_frozen();

  // Is this value an integer?
  inline bool is_integer() const;

  // Is this value a map?
  inline bool is_map() const;

  // Is this value an array?
  inline bool is_array() const;

  // Is this value a seed?
  inline bool is_seed() const;

  // Is this value a string?
  inline bool is_string() const;

  // Is this value a blob?
  inline bool is_blob() const;

  // Is this the null value?
  inline bool is_null() const;

  // Is this a native object?
  inline bool is_native() const;

  // Returns this value viewed as the C type for variants.
  inline pton_variant_t to_c() { return value_; }

  static pton_charset_t default_string_encoding();

protected:
  friend class Sink;
  pton_variant_t value_;

  typedef pton_variant_t::pton_variant_header_t::pton_variant_repr_tag_t repr_tag_t;

  // Convenience accessor for the representation tag.
  repr_tag_t repr_tag() const { return value_.header_.repr_tag_; }

  pton_variant_t::pton_variant_payload_t *payload() { return &value_.payload_; }

  const pton_variant_t::pton_variant_payload_t *payload() const { return &value_.payload_; }

private:
  friend class Arena;

  Variant(repr_tag_t tag, pton_arena_value_t *arena_value);
};

// A variant that represents an array. An array can be either an actual array
// or null, to make conversion more convenient. If you want to be sure you're
// really dealing with an array do an if-check.
class Array : public Variant {
public:
  // Creates a new empty array.
  Array() : Variant() { }

  // Wrap some value as an array.
  Array(Variant variant) : Variant(variant) { }

  // Adds the given value at the end of this array if it is mutable. Returns
  // true if adding succeeded.
  bool add(Variant value) { return array_add(value); }

  // Adds an initially null value to this array, access to which is returned
  // as a sink so setting the sink will cause the array value to be set.
  Sink add();

  // Returns the length of this array.
  uint32_t length() const { return array_length(); }

  // Returns the index'th element, null if the index is greater than the array's
  // length.
  Variant operator[](uint32_t index) const { return array_get(index); }
};

// An iterator that allows you to scan through all the mappings in a map.
class Map_Iterator {
public:
  class Entry {
  public:
    Entry(pton_arena_map_t *data, uint32_t cursor)
      : data_(data)
      , cursor_(cursor) { }
    Variant key() const;
    Variant value() const;
  private:
    friend class Map_Iterator;
    pton_arena_map_t *data_;
    uint32_t cursor_;
  };

  // Advances this iterator to the next entry in this map.
  Map_Iterator &operator++();

  // I don't know what's going on with this. What's the int parameter for?
  Map_Iterator &operator++(int);

  // Returns true iff this and the given iterator point to the same entry.
  bool operator==(const Map_Iterator &that) { return entry_.cursor_ == that.entry_.cursor_; }

  // Returns true iff this and the given iterator point to different entries.
  bool operator!=(const Map_Iterator &that) { return entry_.cursor_ != that.entry_.cursor_; }

  // Returns true iff the next call to advance will return true.
  bool has_next();

  // Returns the current entry.
  const Entry *operator->() { return &entry_; }

private:
  friend class Variant;
  Map_Iterator(pton_arena_map_t *data, uint32_t cursor);
  Map_Iterator() : entry_(NULL, 0) { }

  Entry entry_;
};

// A variant that represents a user-defined seed type.
class Seed : public Variant {
public:
  // This is the name you'd typically use for a seed iterator.
  typedef Map_Iterator Iterator;

  // Creates a new empty seed.
  Seed() : Variant() { }

  // Wrap some value as a seed.
  Seed(Variant variant) : Variant(variant) { }

  // Returns this seed's header.
  Variant header() { return seed_header(); }

  // Sets this seed's header. Returns true iff setting succeeded.
  bool set_header(Variant value) { return seed_set_header(value); }

  // Sets the value of the given field to the given value. Returns true iff
  // setting succeeded.
  bool set_field(Variant key, Variant value) { return seed_set_field(key, value); }

  // Sets the value of the given field to the given value. Returns true iff
  // setting succeeded.
  Variant get_field(Variant key) { return seed_get_field(key); }

  // Returns the number of fields this seed contains.
  size_t field_count() { return seed_field_count(); }

  Iterator fields_begin() { return seed_fields_begin(); }

  Iterator fields_end() { return seed_fields_end(); }

};

// A variant that represents a map. A map can be either an actual map or null,
// to make conversion more convenient. If you want to be sure you're really
// dealing with a map do an if-check.
class Map : public Variant {
public:
  Map() : Variant() { }

  // This is the name you'd typically use for an iterator.
  typedef Map_Iterator Iterator;

  // Wrap some value as a map.
  Map(Variant variant) : Variant(variant) { }

  // Adds a mapping from the given key to the given value if this map is
  // mutable. Returns true if setting succeeded.
  bool set(Variant key, Variant value) { return map_set(key, value); }

  // Adds an open mapping from keys and values to be set later through the
  // sinks returned through the two out parameters.
  bool set(Sink *key_out, Sink *value_out) { return map_set(key_out, value_out); }

  // Returns the mapping for the given key.
  Variant operator[](Variant key) { return map_get(key); }

  // Because of the way string indexing works the normal [] operator can give
  // overloading ambiguities when passed strings. This method disambiguates.
  Variant operator[](const char *str_key) { return map_get(Variant::string(str_key)); }

  // Returns the number of mappings in this map.
  uint32_t size() const { return map_size(); }

  // Returns an iterator at the beginning of this map.
  Iterator begin() const { return map_begin(); }

  // Returns an iterator just past the end of this map.
  Iterator end() const { return map_end(); }
};

// A variant that represents a string. A string can be either an actual string
// or null, to make conversion more convenient. If you want to be sure you're
// really dealing with a string do an if-check.
class String : public Variant {
public:
  String(Variant variant) : Variant(variant) { }

  // Returns the length of this string if it is a string, otherwise 0.
  uint32_t length() const { return string_length(); }

  // Returns this string's character encoding.
  pton_charset_t encoding() const { return string_encoding(); }

  // Returns this string's characters.
  const char *chars() const { return string_chars(); }

  // If this string is mutable, returns the mutable backing array. Otherwise
  // return NULL.
  char *mutable_chars() { return string_mutable_chars(); }
};

// A variant that represents a blob. A blob can be either an actual blob or
// null, to make conversion more convenient. If you want to be sure you're
// really dealing with a blob do an if-check.
class Blob : public Variant {
public:
  Blob() : Variant() { }

  Blob(Variant variant) : Variant(variant) { }

  // Returns the size of this blob if it is a blob, otherwise 0.
  uint32_t size() const { return blob_size(); }

  const void *data() const { return blob_data(); }

  void *mutable_data() { return blob_mutable_data(); }
};

class Native : public Variant {
public:
  Native() : Variant() { }
  Native(Variant variant) : Variant(variant) { }

  // Returns the type of this native object. If this is not a native returns
  // NULL.
  AbstractSeedType *type() { return native_type(); }

  // Returns a raw pointer to the native object. The only really safe way to
  // get access to the value under its type is using ::as so be careful with
  // this one. If this is not a native return NULL.
  void *value() { return native_object(); }

  // Returns this native variant viewed under the given type, but only if this
  // is a native that has that type. If not, NULL is returned.
  template <typename T>
  T *as(ConcreteSeedType<T> *type) { return native_as(type); }
};

// Abstract type of something that can own values. This will almost always be an
// arena but using one as the more abstract variant owner means that you don't
// have to expose the allocation methods in arena, only the ones relevant to
// passing ownership.
class VariantOwner {
protected:
  friend class Arena;
  friend class ArenaData;
  virtual ~VariantOwner() { }

  // Mark this owner as having been adopted.
  virtual void mark_adopted() = 0;

  // Inform this owner that something that had adopted it no longer has.
  virtual void unmark_adopted() = 0;

  // If this owner is really a proxy for another one, return that other one.
  virtual VariantOwner *resolve_adopted() = 0;
};

// A factory is an object that can be used to create new values.
class Factory : public VariantOwner {
public:
  virtual ~Factory() { }

  // Creates and returns a new map value.
  virtual Map new_map() = 0;

  // Creates and returns a new mutable array value.
  virtual Array new_array() = 0;

  // Creates and returns a new mutable array value.
  virtual Array new_array(uint32_t init_capacity) = 0;

  // Creates and returns a new mutable seed value. If a type is specified it
  // is used to initialize the result.
  virtual Seed new_seed(AbstractSeedType *type = NULL) = 0;

  // Creates a new native object of the given type. If no type value is given
  // explicitly then an attempt is made to resolve a default using the
  // default_seed_type struct.
  template <typename T>
  Native new_native(T *object, ConcreteSeedType<T> *type = default_seed_type<T>::get()) {
    return new_raw_native(object, type);
  }

  // Creates a new native plankton object of the given type. This avoids the
  // template cleverness of new_native.
  virtual Native new_raw_native(void *object, AbstractSeedType *type) = 0;

  // Creates and returns a new mutable blob value of the given size.
  virtual Blob new_blob(uint32_t size) = 0;

  // Creates and returns a new sink value that will store the value set into the
  // given output parameter..
  virtual Sink new_sink(Variant *out) = 0;

  // Creates and returns a new variant string with the default encoding. The
  // string is fully owned by the arena so the character array can be disposed
  // after this call returns.
  virtual String new_string(const char *str, uint32_t length) = 0;

  // Creates and returns a new mutable variant string of the given length with
  // the given encoding, initialized to all '\0's. Note that this doesn't mean
  // that the string is initially empty. Variant strings can handle null
  // characters so what you get is a 'length' long string where all the
  // characters are null. The null terminator is implicitly allocated in
  // addition to the requested length, so you only need to worry about the
  // non-null characters.
  virtual String new_string(uint32_t length, pton_charset_t encoding) = 0;

  // Creates and returns a new mutable variant string of the given length with
  // the default encoding, initialized to all '\0's. See the new_string method
  // that takes an explicit encoding for more details.
  virtual String new_string(uint32_t length) = 0;

  // Allocates a raw chunk of memory. Typically you don't want to use this
  // directly but through the 'new' operator, which calls it.
  virtual void *alloc_raw(size_t size) = 0;

  // Assume shared ownership of the values produced in the given arena. After
  // this call, values returned from the given arena will be valid as long as
  // either the given arena _or_ this arena exist. Or, indeed, any other arenas
  // that have also adopted ownership -- ownership of data from an arena can be
  // adopted by an arbitrary number of other arenas. New allocations made within
  // this arena will still only be owned by this one.
  //
  // Note that, importantly, ownership must be linear: so arena A may adopt
  // ownership of values from arena B, or B may adopt ownership of values from
  // A, but if A adopts B _and_ B adopts A they will keep each other alive
  // indefinitely, even after both their scopes have exited, and the memory will
  // leak.
  virtual void adopt_ownership(VariantOwner *owner) = 0;

  // Allocate memory for holding an instance of T* and register a cleanup that
  // calls T's destructor on that instance when this arena is disposed. Note
  // that the result will be uninitialized so you have to initialize it
  // properly, otherwise the behavior of the destructor will not be
  // well-defined.
  template <typename T>
  void *alloc_and_register();

  // Register a cleanup that calls the destructor on the given instance when
  // this arena is disposed. Returns the instance. If you allocate data within
  // an arena that has a nontrivial destructor you should register it to be
  // called using this method, otherwise it won't be. Actually it's not a bad
  // idea to always register instances since even for types that don't need
  // explicit destruction it's easy to change them to require it and forget to
  // change the arena allocation code.
  template <typename T>
  T *register_destructor(T *that);

  // Register a callback to be invoked when this factory is disposed.
  virtual void register_cleanup(tclib::callback_t<void(void)> callback) = 0;
};

// A sink is like a pointer to a variant except that it also has access to an
// arena such that instead of creating a value in an arena and then storing it
// in the sink you would ask the sink to create the value itself.
class Sink {
public:
  Sink() : data_(NULL) { }

  // If this sink has not already been assigned, creates an array, stores it as
  // the value of this sink, and returns it.
  Array as_array();

  // If this sink has not already been assigned, creates a map, stores it as
  // the value of this sink, and returns it.
  Map as_map();

  // If this sink has not already been assigned, creates a seed, stores it as
  // the value of this sink, and returns it.
  Seed as_seed();

  // If this sink has not already been assigned, creates a blob, stores it as
  // the value of this sink, and returns it.
  Blob as_blob(uint32_t size);

  // Returns a factory that can be used to create values that can be stored in
  // this sink. This can be useful in cases where you need a utility value for
  // some sub-computation.
  Factory *factory();

  // Sets the value of this sink, if it hasn't already been set. Otherwise this
  // is a no-op. Returns whether the value was set.
  bool set(Variant value);

  // If this sink has not already been assigned, creates a new string with the
  // given contents and stores it as the value.
  bool set_string(const char *chars, uint32_t length);

  // Returns the C view of this sink.
  pton_sink_t *to_c() { return data_; }

  // Wraps a sink_t struct around a C sink.
  explicit Sink(pton_sink_t *data);

private:
  // Returns true if the data's value can potentially be set.
  bool can_be_set();

  pton_sink_t *data_;
};

// An arena is split into two parts: the arena and the arena's data. The arena
// is really just a thin wrapper around the data which is where the actual state
// is. The data is ref counted so more than one arena can share the same data.
// You don't have to worry too much about this distinction when using arenas but
// the ability to hang on to an arena's data even after the scope that owns the
// arena has exited is useful because it allows ownership to be passed on and
// shared.
class ArenaData : public tclib::refcount_shared_t, VariantOwner {
public:
  ~ArenaData();
  void adopt_ownership(VariantOwner *other);
  void register_cleanup(tclib::callback_t<void(void)> callback);

protected:
  void mark_adopted();
  void unmark_adopted();
  VariantOwner *resolve_adopted();

private:
  friend class Arena;

  // A block of memory allocated within this arena.
  struct block_t {
    uint8_t *memory;
    size_t size;
  };

  // Allocates and returns a block of memory that holds at least the given
  // number of bytes.
  void *alloc_raw(size_t bytes);

  // The raw pages of memory allocated for this arena.
  std::vector<block_t> blocks_;

  // Other arenas this one has adopted.
  std::vector<VariantOwner*> adopted_;

  // Callbacks to call when the arena is disposed.
  std::vector< tclib::callback_t<void(void)> > cleanups_;
};

// An arena within which plankton values can be allocated. Once the values are
// no longer needed all can be disposed by disposing the arena.
class Arena
  : public Factory
  , public tclib::refcount_reference_t<ArenaData>
  , public pton_arena_t {
public:
  // Creates a new empty arena.
  inline Arena();

  // Allocates a new array of values the given size within this arena. Public
  // for testing only. The values are not initialized.
  template <typename T>
  T *alloc_values(uint32_t elms);

  // Allocates a single value of the given type. The value is not initialized.
  template <typename T>
  T *alloc_value();

  // Creates a new native object of the given type.
  Native new_raw_native(void *object, AbstractSeedType *type);

  // Creates and returns a new mutable array value.
  Array new_array();

  // Creates and returns a new mutable array value.
  Array new_array(uint32_t init_capacity);

  // Creates and returns a new mutable map value.
  Map new_map();

  // Creates and returns a new mutable seed value.
  Seed new_seed(AbstractSeedType *type = NULL);

  // Creates and returns a new variant string. The string is fully owned by
  // the arena so the character array can be disposed after this call returns.
  // The length of the string is determined using strlen.
  String new_string(const char *str);

  // Creates and returns a new variant string with the default encoding. The
  // string is fully owned by the arena so the character array can be disposed
  // after this call returns.
  String new_string(const char *str, uint32_t length);

  // Creates and returns a new variant string with the given encoding. The
  // string is fully owned by the arena so the character array can be disposed
  // after this call returns.
  String new_string(const void *str, uint32_t length, pton_charset_t encoding);

  // Creates and returns a new mutable variant string of the given length with
  // the default encoding, initialized to all '\0's. See the new_string method
  // that takes an explicit encoding for more details.
  String new_string(uint32_t length);

  // Creates and returns a new mutable variant string of the given length with
  // the given encoding, initialized to all '\0's. Note that this doesn't mean
  // that the string is initially empty. Variant strings can handle null
  // characters so what you get is a 'length' long string where all the
  // characters are null. The null terminator is implicitly allocated in
  // addition to the requested length, so you only need to worry about the
  // non-null characters.
  String new_string(uint32_t length, pton_charset_t encoding);

  // Creates and returns a new variant blob. The contents it copied into this
  // arena so the data array can be disposed after this call returns.
  Blob new_blob(const void *data, uint32_t size);

  // Creates and returns a new mutable variant blob of the given size,
  // initialized to all zeros.
  Blob new_blob(uint32_t size);

  // Creates and returns a new sink value that will store the value set into the
  // given output parameter.
  Sink new_sink(plankton::Variant *out);

  // Assume shared ownership of the values produced in the given arena. After
  // this call, values returned from the given arena will be valid as long as
  // either the given arena _or_ this arena exist. Or, indeed, any other arenas
  // that have also adopted ownership -- ownership of data from an arena can be
  // adopted by an arbitrary number of other arenas. New allocations made within
  // this arena will still only be owned by this one.
  //
  // Note that, importantly, ownership must be linear: so arena A may adopt
  // ownership of values from arena B, or B may adopt ownership of values from
  // A, but if A adopts B _and_ B adopts A they will keep each other alive
  // indefinitely, even after both their scopes have exited, and the memory will
  // leak.
  void adopt_ownership(VariantOwner *owner);

  // Allocates a raw block of memory.
  void *alloc_raw(size_t size);

  // Register a callback to be invoked when this factory is disposed.
  virtual void register_cleanup(tclib::callback_t<void(void)> callback);

  // Given a C arena, returns the C++ view of it.
  static Arena *from_c(pton_arena_t *c_arena) {
    return static_cast<Arena*>(c_arena);
  }

protected:
  // Hook into the ownership framework.
  void mark_adopted();
  void unmark_adopted();
  VariantOwner *resolve_adopted();

private:
  friend pton_sink_t *pton_new_sink(pton_arena_t *arena);
  friend struct ::pton_arena_array_t;
  friend struct ::pton_arena_map_t;

  ArenaData *data();

  // Allocates the backing storage for a sink value.
  template <typename S>
  S *alloc_sink();
};

} // namespace plankton

#endif // _PLANKTON_HH
