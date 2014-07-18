//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "plankton-inl.hh"
#include "stdc.h"
#include <new>

namespace plankton {

// An arena-allocated array.
class arena_array_t {
public:
  explicit arena_array_t(arena_t *origin);

  // Appends the given element to this array.
  arena_array_t *add(variant_t value);

  variant_t operator[](size_t index);

  size_t length() { return length_; }

private:
  arena_t *origin_;
  size_t length_;
  size_t capacity_;
  variant_t *elms_;
};

class arena_map_t {
public:
  struct entry_t {
    variant_t key;
    variant_t value;
  };

  explicit arena_map_t(arena_t *origin);

  arena_map_t *set(variant_t key, variant_t value);

  variant_t operator[](variant_t key);

  size_t size() { return size_; }

private:
  friend class map_t::iterator;

  arena_t *origin_;
  size_t size_;
  size_t capacity_;
  entry_t *elms_;
};

class arena_string_t {
public:
  explicit arena_string_t(const char *chars, size_t length);

  size_t length() { return length_; }

  const char *chars() { return chars_; }

private:
  const char *chars_;
  size_t length_;
};

class arena_blob_t {
public:
  explicit arena_blob_t(const void *data, size_t size);

  size_t size() { return size_; }

  const void *data() { return data_; }

private:
  const void *data_;
  size_t size_;
};

class arena_sink_t {
public:
  explicit arena_sink_t(arena_t *origin);

  bool set(variant_t value);

  variant_t operator*();

private:
  bool is_empty_;
  variant_t value_;
  arena_t *origin_;
};

void *arena_t::alloc_raw(size_t bytes) {
  if (used_ == capacity_) {
    // Expand capacity if necessary. This is also where we start since the
    // initial used and capacity are 0.
    capacity_ = ((capacity_ < 16) ? 16 : (capacity_ * 2));
    uint8_t **new_blocks = new uint8_t*[capacity_];
    memcpy(new_blocks, blocks_, sizeof(uint8_t*) * used_);
    delete[] blocks_;
    blocks_ = new_blocks;
  }
  uint8_t *result = new uint8_t[bytes];
  blocks_[used_++] = result;
  return result;
}

arena_t::~arena_t() {
  for (size_t i = 0; i < used_; i++)
    delete[] blocks_[i];
  delete[] blocks_;
}

array_t arena_t::new_array() {
  arena_array_t *data = alloc_value<arena_array_t>();
  return array_t(new (data) arena_array_t(this));
}

map_t arena_t::new_map() {
  arena_map_t *data = alloc_value<arena_map_t>();
  return map_t(new (data) arena_map_t(this));
}

variant_t arena_t::new_string(const char *str) {
  return new_string(str, strlen(str));
}

variant_t arena_t::new_string(const char *str, size_t length) {
  arena_string_t *data = alloc_value<arena_string_t>();
  char *own_str = alloc_values<char>(length + 1);
  memcpy(own_str, str, length);
  own_str[length] = '\0';
  return variant_t(new (data) arena_string_t(own_str, length));
}

variant_t arena_t::new_blob(const void *start, size_t size) {
  arena_blob_t *data = alloc_value<arena_blob_t>();
  uint8_t *own_start = alloc_values<uint8_t>(size);
  memcpy(own_start, start, size);
  return variant_t(new (data) arena_blob_t(own_start, size));
}

sink_t arena_t::new_sink() {
  arena_sink_t *data = alloc_sink();
  return sink_t(data);
}

arena_sink_t *arena_t::alloc_sink() {
  arena_sink_t *result = alloc_value<arena_sink_t>();
  return new (result) arena_sink_t(this);
}

variant_t::type_t variant_t::type() const {
  return static_cast<type_t>(repr_tag_ >> 4);
}

bool variant_t::operator==(variant_t that) {
  type_t this_type = type();
  type_t that_type = that.type();
  if (this_type != that_type)
    return false;
  switch (this_type) {
    case vtInteger:
      return integer_value() == that.integer_value();
    case vtString: {
      size_t length = string_length();
      if (that.string_length() != length)
        return false;
      return strncmp(string_chars(), that.string_chars(), length) == 0;
    }
    case vtBlob: {
      size_t size = blob_size();
      if (that.blob_size() != size)
        return false;
      return strncmp(static_cast<const char*>(blob_data()),
          static_cast<const char*>(that.blob_data()), size) == 0;
    }
    case vtArray:
      return data_.as_arena_array_ == that.data_.as_arena_array_;
    case vtMap:
      return data_.as_arena_map_ == that.data_.as_arena_map_;
    case vtNull:
      return true;
    case vtBool:
      return repr_tag_ == that.repr_tag_;
    default:
      return false;
  }
}

variant_t variant_t::blob(const void *data, size_t size) {
  variant_t result;
  result.repr_tag_ = rtExternalBlob;
  result.data_.as_external_blob_.data_ = data;
  result.data_.as_external_blob_.size_ = size;
  return result;
}

uint32_t variant_t::blob_size() const {
  switch (repr_tag_) {
    case rtExternalBlob:
      return data_.as_external_blob_.size_;
    case rtArenaBlob:
      return data_.as_arena_blob_->size();
    default:
      return 0;
  }
}

const void *variant_t::blob_data() const {
  switch (repr_tag_) {
    case rtExternalBlob:
      return data_.as_external_blob_.data_;
    case rtArenaBlob:
      return data_.as_arena_blob_->data();
    default:
      return NULL;
  }
}

array_t::array_t(arena_array_t *data) : variant_t(data) { }

array_t::array_t(variant_t variant) : variant_t() {
  // Initialize this to the null value and then, if the given variant is an
  // array, override with the variant's state.
  if (variant.type() == vtArray)
    *static_cast<variant_t*>(this) = variant;
}

array_t array_t::add(variant_t value) {
  if (*this)
    data()->add(value);
  return *this;
}

size_t array_t::length() {
  return *this ? data()->length() : 0;
}

variant_t array_t::operator[](size_t index) {
  return *this ? data()->operator[](index) : null();
}

arena_array_t *array_t::data() {
  return data_.as_arena_array_;
}

arena_array_t::arena_array_t(arena_t *origin)
  : origin_(origin)
  , length_(0)
  , capacity_(0)
  , elms_(NULL) { }

arena_array_t *arena_array_t::add(variant_t value) {
  if (length_ == capacity_) {
    capacity_ = (capacity_ < 4 ? 4 : (2 * capacity_));
    variant_t *new_elms = origin_->alloc_values<variant_t>(capacity_);
    memcpy(new_elms, elms_, sizeof(variant_t) * length_);
    elms_ = new_elms;
  }
  elms_[length_++] = value;
  return this;
}

variant_t arena_array_t::operator[](size_t index) {
  return (index < length_) ? elms_[index] : variant_t::null();
}

map_t::map_t(arena_map_t *data) : variant_t(data) { }

map_t::map_t(variant_t variant) : variant_t() {
  // Initialize this to the null value and then, if the given variant is a map,
  // override with the variant's state.
  if (variant.type() == vtMap)
    *static_cast<variant_t*>(this) = variant;
}

size_t map_t::size() {
  return *this ? data()->size() : 0;
}

map_t map_t::set(variant_t key, variant_t value) {
  if (*this)
    data()->set(key, value);
  return *this;
}

variant_t map_t::operator[](variant_t key) {
  return *this ? data()->operator[](key) : variant_t::null();
}

arena_map_t *map_t::data() {
  return data_.as_arena_map_;
}

bool map_t::iterator::advance(variant_t *key, variant_t *value) {
  if (cursor_ == data_->size()) {
    return false;
  } else {
    *key = data_->elms_[cursor_].key;
    *value = data_->elms_[cursor_].value;
    cursor_++;
    return true;
  }
}

bool map_t::iterator::has_next() {
  return cursor_ < data_->size();
}

arena_map_t::arena_map_t(arena_t *origin)
  : origin_(origin)
  , size_(0)
  , capacity_(0)
  , elms_(NULL) { }

arena_map_t *arena_map_t::set(variant_t key, variant_t value) {
  if (size_ == capacity_) {
    capacity_ = (capacity_ < 4 ? 4 : (2 * capacity_));
    entry_t *new_elms = origin_->alloc_values<entry_t>(capacity_);
    memcpy(new_elms, elms_, sizeof(entry_t) * size_);
    elms_ = new_elms;
  }
  entry_t *entry = &elms_[size_++];
  entry->key = key;
  entry->value = value;
  return this;
}

variant_t arena_map_t::operator[](variant_t key) {
  for (size_t i = 0; i < size_; i++) {
    entry_t *entry = &elms_[i];
    if (entry->key == key)
      return entry->value;
  }
  return variant_t::null();
}

uint32_t variant_t::string_length() const {
  switch (repr_tag_) {
    case rtExternalString:
      return data_.as_external_string_.length_;
    case rtArenaString:
      return data_.as_arena_string_->length();
    default:
      return 0;
  }
}

const char *variant_t::string_chars() const {
  switch (repr_tag_) {
    case rtExternalString:
      return data_.as_external_string_.chars_;
    case rtArenaString:
      return data_.as_arena_string_->chars();
    default:
      return NULL;
  }
}

arena_string_t::arena_string_t(const char *chars, size_t length)
  : chars_(chars)
  , length_(length) { }

arena_blob_t::arena_blob_t(const void *data, size_t size)
  : data_(data)
  , size_(size) { }

sink_t::sink_t(arena_sink_t *data)
  : data_(data) { }

variant_t sink_t::operator*() const {
  return data_->operator*();
}

bool sink_t::set(variant_t value) {
  return data_->set(value);
}

arena_sink_t::arena_sink_t(arena_t *origin)
  : is_empty_(true)
  , origin_(origin) { }

variant_t arena_sink_t::operator*() {
  return value_;
}

bool arena_sink_t::set(variant_t value) {
  if (is_empty_) {
    is_empty_ = false;
    value_ = value;
    return true;
  } else {
    return false;
  }
}

} // namespace plankton
