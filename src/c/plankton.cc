//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "plankton-inl.hh"
#include "stdc.h"
#include <new>

using namespace plankton;

// Shared between all the arena types.
class pton_arena_value_t {
public:
  pton_arena_value_t() : is_frozen_(false) { }

  bool is_frozen() { return is_frozen_; }

  void ensure_frozen() { is_frozen_ = true; }

protected:
  bool is_frozen_;
};

// An arena-allocated array.
class pton_arena_array_t : public pton_arena_value_t {
public:
  pton_arena_array_t(pton_arena_t *origin, size_t init_capacity);

  bool add(variant_t value);

  variant_t get(size_t index);

  size_t length() { return length_; }

private:
  friend class pton_arena_t;
  static const size_t kDefaultInitCapacity = 8;
  pton_arena_t *origin_;
  size_t length_;
  size_t capacity_;
  variant_t *elms_;
};

struct pton_arena_map_t : public pton_arena_value_t {
public:
  struct entry_t {
    variant_t key;
    variant_t value;
  };

  explicit pton_arena_map_t(pton_arena_t *origin);

  bool set(variant_t key, variant_t value);

  variant_t get(variant_t key) const;

  uint32_t size() const { return size_; }

private:
  friend class ::map_iterator_t;

  pton_arena_t *origin_;
  uint32_t size_;
  uint32_t capacity_;
  entry_t *elms_;
};

class pton_arena_string_t : public pton_arena_value_t {
public:
  pton_arena_string_t(char *chars, size_t length, bool is_frozen);

  size_t length() { return length_; }

  const char *chars() { return chars_; }

  bool set(size_t index, char c);

private:
  char *chars_;
  size_t length_;
};

class pton_arena_blob_t : public pton_arena_value_t {
public:
  pton_arena_blob_t(void *data, uint32_t size, bool is_frozen);

  uint32_t size() { return size_; }

  void *data() { return data_; }

private:
  void *data_;
  uint32_t size_;
};

class pton_arena_sink_t {
public:
  explicit pton_arena_sink_t(pton_arena_t *origin);

  bool set(variant_t value);

  variant_t operator*();

private:
  bool is_empty_;
  variant_t value_;
  pton_arena_t *origin_;
};

void *pton_arena_t::alloc_raw(size_t bytes) {
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

pton_arena_t::~pton_arena_t() {
  for (size_t i = 0; i < used_; i++)
    delete[] blocks_[i];
  delete[] blocks_;
}

array_t pton_arena_t::new_array() {
  return new_array(pton_arena_array_t::kDefaultInitCapacity);
}

array_t pton_arena_t::new_array(size_t init_capacity) {
  pton_arena_array_t *data = alloc_value<pton_arena_array_t>();
  variant_t result(variant_p::rtArenaArray, new (data) pton_arena_array_t(this, init_capacity));
  return array_t(result);
}

map_t pton_arena_t::new_map() {
  pton_arena_map_t *data = alloc_value<pton_arena_map_t>();
  variant_t result(variant_p::rtArenaMap, new (data) pton_arena_map_t(this));
  return map_t(result);
}

string_t pton_arena_t::new_string(const char *str) {
  return new_string(str, strlen(str));
}

string_t pton_arena_t::new_string(const char *str, size_t length) {
  pton_arena_string_t *data = alloc_value<pton_arena_string_t>();
  char *own_str = alloc_values<char>(length + 1);
  memcpy(own_str, str, length);
  own_str[length] = '\0';
  variant_t result(variant_p::rtArenaString, new (data) pton_arena_string_t(own_str, length, true));
  return string_t(result);
}

string_t pton_arena_t::new_string(size_t length) {
  pton_arena_string_t *data = alloc_value<pton_arena_string_t>();
  char *own_str = alloc_values<char>(length + 1);
  memset(own_str, '\0', length + 1);
  variant_t result(variant_p::rtArenaString, new (data) pton_arena_string_t(own_str, length, false));
  return string_t(result);
}

blob_t pton_arena_t::new_blob(const void *start, size_t size) {
  pton_arena_blob_t *data = alloc_value<pton_arena_blob_t>();
  uint8_t *own_start = alloc_values<uint8_t>(size);
  memcpy(own_start, start, size);
  variant_t result(variant_p::rtArenaBlob, new (data) pton_arena_blob_t(own_start, size, true));
  return blob_t(result);
}

blob_t pton_arena_t::new_blob(size_t size) {
  pton_arena_blob_t *data = alloc_value<pton_arena_blob_t>();
  uint8_t *bytes = alloc_values<uint8_t>(size);
  memset(bytes, 0, size);
  variant_t result(variant_p::rtArenaBlob, new (data) pton_arena_blob_t(data, size, false));
  return blob_t(result);
}

sink_t pton_arena_t::new_sink() {
  pton_arena_sink_t *data = alloc_sink();
  return sink_t(data);
}

pton_arena_sink_t *pton_arena_t::alloc_sink() {
  pton_arena_sink_t *result = alloc_value<pton_arena_sink_t>();
  return new (result) pton_arena_sink_t(this);
}

variant_t::type_t variant_t::type() const {
  return static_cast<type_t>(repr_tag() >> 4);
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
      return payload()->as_arena_array_ == that.payload()->as_arena_array_;
    case vtMap:
      return payload()->as_arena_map_ == that.payload()->as_arena_map_;
    case vtNull:
      return true;
    case vtBool:
      return repr_tag() == that.repr_tag();
    default:
      return false;
  }
}

bool variant_t::is_frozen() {
  switch (repr_tag()) {
    case variant_p::rtInteger:
    case variant_p::rtNull:
    case variant_p::rtTrue:
    case variant_p::rtFalse:
    case variant_p::rtExternalString:
    case variant_p::rtExternalBlob:
      return true;
    case variant_p::rtArenaArray:
    case variant_p::rtArenaMap:
    case variant_p::rtArenaString:
    case variant_p::rtArenaBlob:
      return payload()->as_arena_value_->is_frozen();
    default:
      return false;
  }
}

void variant_t::ensure_frozen() {
  switch (repr_tag()) {
    case variant_p::rtArenaArray:
    case variant_p::rtArenaMap:
    case variant_p::rtArenaString:
    case variant_p::rtArenaBlob:
      payload()->as_arena_value_->ensure_frozen();
      break;
    default:
      break;
  }
}

variant_t variant_t::blob(const void *data, size_t size) {
  variant_t result;
  result.value_.repr_tag_ = variant_p::rtExternalBlob;
  result.payload()->as_external_blob_.data_ = data;
  result.payload()->as_external_blob_.size_ = size;
  return result;
}

array_t::array_t(variant_t variant) : variant_t() {
  // Initialize this to the null value and then, if the given variant is an
  // array, override with the variant's state.
  if (variant.is_array())
    *static_cast<variant_t*>(this) = variant;
}

bool variant_t::array_add(variant_t value) {
  return is_array() ? payload()->as_arena_array_->add(value) : false;
}

uint32_t variant_t::array_length() const {
  return is_array() ? payload()->as_arena_array_->length() : 0;
}

variant_t variant_t::array_get(size_t index) const {
  return is_array() ? payload()->as_arena_array_->get(index) : null();
}

pton_arena_array_t::pton_arena_array_t(pton_arena_t *origin, size_t init_capacity)
  : origin_(origin)
  , length_(0)
  , capacity_(0)
  , elms_(NULL) {
  if (init_capacity < kDefaultInitCapacity)
    init_capacity = kDefaultInitCapacity;
  capacity_ = init_capacity;
  elms_ = origin->alloc_values<variant_t>(capacity_);
}

bool pton_arena_array_t::add(variant_t value) {
  if (is_frozen())
    return false;
  if (length_ == capacity_) {
    capacity_ *= 2;
    variant_t *new_elms = origin_->alloc_values<variant_t>(capacity_);
    memcpy(new_elms, elms_, sizeof(variant_t) * length_);
    elms_ = new_elms;
  }
  elms_[length_++] = value;
  return true;
}

variant_t pton_arena_array_t::get(size_t index) {
  return (index < length_) ? elms_[index] : variant_t::null();
}

map_t::map_t(variant_t variant) : variant_t() {
  // Initialize this to the null value and then, if the given variant is a map,
  // override with the variant's state.
  if (variant.type() == vtMap)
    *static_cast<variant_t*>(this) = variant;
}

uint32_t variant_t::map_size() const {
  return is_map() ? payload()->as_arena_map_->size() : 0;
}

bool variant_t::map_set(variant_t key, variant_t value) {
  return is_map() ? payload()->as_arena_map_->set(key, value) : false;
}

variant_t variant_t::map_get(variant_t key) const {
  return is_map() ? payload()->as_arena_map_->get(key) : variant_t::null();
}

map_iterator_t variant_t::map_iter() const {
  return is_map() ? map_iterator_t(payload()->as_arena_map_) : map_iterator_t();
}

map_iterator_t::map_iterator_t(pton_arena_map_t *data)
  : data_(data)
  , cursor_(0)
  , limit_(data->size_) { }

bool map_iterator_t::advance(variant_t *key, variant_t *value) {
  if (cursor_ == limit_) {
    return false;
  } else {
    *key = data_->elms_[cursor_].key;
    *value = data_->elms_[cursor_].value;
    cursor_++;
    return true;
  }
}

bool map_iterator_t::has_next() {
  return cursor_ < data_->size();
}

pton_arena_map_t::pton_arena_map_t(pton_arena_t *origin)
  : origin_(origin)
  , size_(0)
  , capacity_(0)
  , elms_(NULL) { }

bool pton_arena_map_t::set(variant_t key, variant_t value) {
  if (is_frozen())
    return false;
  if (size_ == capacity_) {
    capacity_ = (capacity_ < 4 ? 4 : (2 * capacity_));
    entry_t *new_elms = origin_->alloc_values<entry_t>(capacity_);
    memcpy(new_elms, elms_, sizeof(entry_t) * size_);
    elms_ = new_elms;
  }
  entry_t *entry = &elms_[size_++];
  entry->key = key;
  entry->value = value;
  return true;
}

variant_t pton_arena_map_t::get(variant_t key) const {
  for (size_t i = 0; i < size_; i++) {
    entry_t *entry = &elms_[i];
    if (entry->key == key)
      return entry->value;
  }
  return variant_t::null();
}

uint32_t variant_t::string_length() const {
  switch (repr_tag()) {
    case variant_p::rtExternalString:
      return payload()->as_external_string_.length_;
    case variant_p::rtArenaString:
      return payload()->as_arena_string_->length();
    default:
      return 0;
  }
}

const char *variant_t::string_chars() const {
  switch (repr_tag()) {
    case variant_p::rtExternalString:
      return payload()->as_external_string_.chars_;
    case variant_p::rtArenaString:
      return payload()->as_arena_string_->chars();
    default:
      return NULL;
  }
}

char variant_t::string_get(size_t index) const {
  if (index >= string_length())
    // Only real strings have nonzero length so this all non-strings end up
    // here.
    return 0;
  return string_chars()[index];
}

bool variant_t::string_set(size_t index, char c) {
  if (is_frozen() || index >= string_length())
    // Only strings have nonzero length and only arena strings can be mutable
    // so this handles all other cases.
    return false;
  return payload()->as_arena_string_->set(index, c);
}

string_t::string_t(variant_t variant) {
  if (variant.is_string())
    *static_cast<variant_t*>(this) = variant;
}

pton_arena_string_t::pton_arena_string_t(char *chars, size_t length, bool is_frozen)
  : chars_(chars)
  , length_(length) {
  is_frozen_ = is_frozen;
}

bool pton_arena_string_t::set(size_t index, char c) {
  if (index >= length_ || is_frozen_)
    return false;
  chars_[index] = c;
  return true;
}

pton_arena_blob_t::pton_arena_blob_t(void *data, uint32_t size, bool is_frozen)
  : data_(data)
  , size_(size) {
  is_frozen_ = is_frozen;
}

uint32_t variant_t::blob_size() const {
  switch (repr_tag()) {
    case variant_p::rtExternalBlob:
      return payload()->as_external_blob_.size_;
    case variant_p::rtArenaBlob:
      return payload()->as_arena_blob_->size();
    default:
      return 0;
  }
}

const void *variant_t::blob_data() const {
  switch (repr_tag()) {
    case variant_p::rtExternalBlob:
      return payload()->as_external_blob_.data_;
    case variant_p::rtArenaBlob:
      return payload()->as_arena_blob_->data();
    default:
      return NULL;
  }
}

uint8_t variant_t::blob_get(size_t index) const {
  if (index >= blob_size())
    // Only real blobs have nonzero size so this all non-blobs end up here.
    return 0;
  return static_cast<const uint8_t*>(blob_data())[index];
}

bool variant_t::blob_set(size_t index, uint8_t b) {
  if (is_frozen() || index >= blob_size())
    // Only blobs have nonzero size and only arena blobs can be mutable so this
    // handles all other cases.
    return false;
  static_cast<uint8_t*>(payload()->as_arena_blob_->data())[index] = b;
  return true;
}

blob_t::blob_t(variant_t variant) {
  if (variant.is_blob())
    *static_cast<variant_t*>(this) = variant;
}

sink_t::sink_t(pton_arena_sink_t *data)
  : data_(data) { }

variant_t sink_t::operator*() const {
  return data_->operator*();
}

bool sink_t::set(variant_t value) {
  return data_->set(value);
}

pton_arena_sink_t::pton_arena_sink_t(pton_arena_t *origin)
  : is_empty_(true)
  , origin_(origin) { }

variant_t pton_arena_sink_t::operator*() {
  return value_;
}

bool pton_arena_sink_t::set(variant_t value) {
  if (is_empty_) {
    is_empty_ = false;
    value_ = value;
    return true;
  } else {
    return false;
  }
}
