//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// C++ implementation of the plankton serialization format.

#ifndef _PLANKTON
#define _PLANKTON

#include "stdc.h"

namespace plankton {

class arena_array_t;
class arena_blob_t;
class arena_value_t;
class arena_map_t;
class arena_string_t;
class arena_sink_t;
class arena_t;

// A plankton variant.
class variant_t {
private:
  // Tags that identify the internal representation of variants.
  enum repr_tag_t {
    rtInteger = 0x10,
    rtExternalString = 0x20,
    rtArenaString = 0x21,
    rtExternalBlob = 0x30,
    rtArenaBlob = 0x31,
    rtNull = 0x40,
    rtTrue = 0x50,
    rtFalse = 0x60,
    rtArenaArray = 0x70,
    rtArenaMap = 0x80
  };

public:
  // The different types of variants. The values are the corresponding
  // representation tags downshifted by 4.
  enum type_t {
    vtInteger = 0x01,
    vtString = 0x02,
    vtBlob = 0x03,
    vtNull = 0x04,
    vtTrue = 0x05,
    vtFalse = 0x06,
    vtArray = 0x07,
    vtMap = 0x08
  };

  // Initializes a variant representing null.
  inline variant_t() : repr_tag_(rtNull) { data_.as_integer_ = 0; }

  // Static method that returns a variant representing null. Equivalent to
  // the no-arg constructor but more explicit.
  static inline variant_t null() { return variant_t(); }

  // Returns a variant representing the boolean true. Called 'yes' because
  // 'true' is a keyword.
  static inline variant_t yes() { return variant_t(rtTrue); }

  // Returns a variant representing the boolean false. Called 'no' because
  // 'false' is a keyword.
  static inline variant_t no() { return variant_t(rtFalse); }

  // Returns a variant representing a bool, false if the value is 0, true
  // otherwise.
  static inline variant_t boolean(int value) { return variant_t(value ? rtTrue : rtFalse); }

  // Initializes a variant representing an integer with the given value. Note
  // that this is funky when used with a literal 0 because it also matches the
  // pointer constructors.
  inline variant_t(int64_t integer);

  // Static constructor for integer variants that doesn't rely on overloading,
  // unlike the constructor.
  static inline variant_t integer(int64_t value);

  // Initializes a variant representing a string with the given contents, using
  // strlen to determine the string's length. This does not copy the string so
  // it has to stay alive for as long as the variant is used.
  inline variant_t(const char *string);

  // Explicit constructor for string-valued variants. Note that the variant does
  // not take ownership of the string so it must stay alive as long as the
  // variant does. Use an arena to create a variant that does take ownership.
  static inline variant_t string(const char *string);

  // Initializes a variant representing a string with the given contents. This
  // does not copy the string so it has to stay alive for as long as the
  // variant is used. Use an arena to create a variant that does copy the string.
  inline variant_t(const char *string, size_t length);

  // Explicit constructor for string-valued variants. Note that the variant does
  // not take ownership of the string so it must stay alive as long as the
  // variant does. Use an arena to create a variant that does take ownership.
  static inline variant_t string(const char *string, size_t length);

  // Explicit constructor for a binary blob. The size is in bytes. This
  // does not copy the string so it has to stay alive for as long as the
  // variant is used. Use an arena to create a variant that does copy the string.
  static variant_t blob(const void *data, size_t size);

  // Returns this value't type.
  type_t type() const;

  // Returns the integer value of this variant if it is an integer, otherwise
  // 0.
  inline int64_t integer_value() const;

  // Returns the length of this string if it is a string, otherwise 0.
  uint32_t string_length() const;

  // Returns the characters of this string if it is a string, otherwise NULL.
  const char *string_chars() const;

  // If this variant is a blob, returns the number of bytes. If not, returns 0.
  uint32_t blob_size() const;

  // If this variant is a blob returns the blob data. If not returns NULL.
  const void *blob_data() const;

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
  bool operator==(variant_t that);

protected:
  repr_tag_t repr_tag_;
  union {
    int64_t as_integer_;
    struct {
      uint32_t length_;
      const char *chars_;
    } as_external_string_;
    struct {
      uint32_t size_;
      const void *data_;
    } as_external_blob_;
    arena_array_t *as_arena_array_;
    arena_map_t *as_arena_map_;
    arena_string_t *as_arena_string_;
    arena_blob_t *as_arena_blob_;
  } data_;

  inline bool is_integer() const { return repr_tag_ == rtInteger; }

  inline variant_t(arena_array_t *arena_array);

  inline variant_t(arena_map_t *arena_map);

private:
  friend class arena_t;

  // Creates a variant with no payload and the given type.
  variant_t(repr_tag_t tag) : repr_tag_(tag) { data_.as_integer_ = 0; }

  inline variant_t(arena_string_t *arena_string);

  inline variant_t(arena_blob_t *arena_blob);
};

// A variant that represents an array. An array can be either an actual array
// or null, to make conversion more convenient. If you want to be sure you're
// really dealing with an array do an if-check.
class array_t : public variant_t {
public:
  // Conversion to an array of some value. If the value is indeed an array the
  // result is a proper array, if it is something else the result is null.
  explicit array_t(variant_t variant);

  // Adds the given value at the end of this array.
  array_t add(variant_t value);

  // Returns the length of this array.
  size_t length();

  // Returns the index'th element, null if the index is greater than the array's
  // length.
  variant_t operator[](size_t index);

private:
  friend class arena_t;
  explicit array_t(arena_array_t *data);

  // Returns this array's underlying data.
  arena_array_t *data();
};

// A variant that represents a map. A map can be either an actual map or null,
// to make conversion more convenient. If you want to be sure you're really
// dealing with a map do an if-check.
class map_t : public variant_t {
public:
  // An iterator that allows you to scan through all the mappings in a map.
  class iterator {
  public:
    // If there are more mappings in this map sets the key and value in the
    // given out parameters and returns true. Otherwise returns false.
    bool advance(variant_t *key, variant_t *value);

    // Returns true iff the next call to advance will return true.
    bool has_next();

  private:
    friend class map_t;
    iterator(arena_map_t *data) : data_(data), cursor_(0) { }

    arena_map_t *data_;
    size_t cursor_;
  };

  // Conversion to a map of some value. If the value is indeed a map the
  // result is a proper map, if it is something else the result is null.
  explicit map_t(variant_t variant);

  // Adds a mapping from the given key to the given value.
  map_t set(variant_t key, variant_t value);

  // Returns the mapping for the given key.
  variant_t operator[](variant_t key);

  // Returns the number of mappings in this map.
  size_t size();

  // Returns an iterator for iterating this map. The first call to advance will
  // yield the first mapping, if there is one.
  iterator iter() { return iterator(data()); }

private:
  friend class arena_t;
  explicit map_t(arena_map_t *data);

  // Returns this map's underlying data.
  arena_map_t *data();
};

// A sink is like a pointer to a variant except that it also has access to an
// arena such that instead of creating a value in an arena and then storing it
// in the sink you would ask the sink to create the value itself.
class sink_t {
public:
  // Returns the value stored in this sink.
  variant_t operator*() const;

  // If this sink has not already been asigned, creates an array, stores it as
  // the value of this sink, and returns it.
  array_t as_array();

  // If this sink has not already been asigned, creates a map, stores it as
  // the value of this sink, and returns it.
  map_t as_map();

  // Sets the value of this sink, if it hasn't already been set. Otherwise this
  // is a no-op. Returns whether the value was set.
  bool set(variant_t value);

private:
  friend class arena_t;
  explicit sink_t(arena_sink_t *data);

  arena_sink_t *data_;
};

// An arena within which plankton values can be allocated. Once the values are
// no longer needed all can be disposed by disposing the arena.
class arena_t {
public:
  // Creates a new empty arena.
  inline arena_t();

  // Disposes all memory allocated within this arena.
  ~arena_t();

  // Allocates a new array of values the given size within this arena. Public
  // for testing only. The values are not initialized.
  template <typename T>
  T *alloc_values(size_t elms);

  // Allocates a single value of the given type. The value is not initialized.
  template <typename T>
  T *alloc_value();

  // Creates and returns a new array value.
  array_t new_array();

  // Creates and returns a new map value.
  map_t new_map();

  // Creates and returns a new variant string. The string is fully owned by
  // the arena so the character array can be disposed after this call returns.
  // The length of the string is determined using strlen.
  variant_t new_string(const char *str);

  // Creates and returns a new variant string. The string is fully owned by
  // the arena so the character array can be disposed after this call returns.
  variant_t new_string(const char *str, size_t length);

  // Creates and returns a new variant blob. The contents it copied into this
  // arena so the data array can be disposed after this call returns.
  variant_t new_blob(const void *data, size_t size);

  // Creates and returns a new sink value.
  sink_t new_sink();

private:
  // Allocates a raw block of memory.
  void *alloc_raw(size_t size);

  // Allocates the backing storage for a sink value.
  arena_sink_t *alloc_sink();

  size_t capacity_;
  size_t used_;
  uint8_t **blocks_;
};

// An object that holds the representation of a variant as a 7-bit ascii string.
class TextWriter {
public:
  TextWriter();
  ~TextWriter();

  // Write the given variant to this asciigram.
  void write(variant_t value);

  // After encoding, returns the string containing the encoded representation.
  const char *operator*() { return chars_; }

  // After encoding, returns the length of the string containing the encoded
  // representation.
  size_t length() { return length_; }

private:
  friend class AsciiEncoder;
  char *chars_;
  size_t length_;
};

// Utility for converting a plankton variant to a 7-bit ascii string.
class TextParser {
public:
  // Creates a new parser which uses the given arena for allocation.
  TextParser(arena_t *arena);

  // Parse the given input, returning the value. If any errors occur the
  // has_failed() and offender() methods can be used to identify what the
  // problem was.
  variant_t parse(const char *chars, size_t length);

  // Returns true iff the last parse failed.  If parse hasn't been called at all
  // returns false.
  bool has_failed() { return has_failed_; }

  // If has_failed() returns true this will return the offending character.
  char offender() { return offender_; }

private:
  friend class AsciiDecoder;
  arena_t *arena_;
  bool has_failed_;
  char offender_;
};

} // namespace plankton

#endif // _PLANKTON
