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
  pton_arena_array_t(pton_arena_t *origin, uint32_t init_capacity);

  bool add(variant_t value);

  variant_t get(uint32_t index);

  uint32_t length() { return length_; }

private:
  friend class pton_arena_t;
  static const uint32_t kDefaultInitCapacity = 8;
  pton_arena_t *origin_;
  uint32_t length_;
  uint32_t capacity_;
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
  pton_arena_string_t(char *chars, uint32_t length, bool is_frozen);

  uint32_t length() { return length_; }

  const char *chars() { return chars_; }

  bool set(uint32_t index, char c);

private:
  char *chars_;
  uint32_t length_;
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

class pton_sink_t {
public:
  explicit pton_sink_t(pton_arena_t *origin);

  bool set(variant_t value);

  variant_t operator*();

private:
  bool is_empty_;
  variant_t value_;
  pton_arena_t *origin_;
};

pton_arena_t *pton_new_arena() {
  return new arena_t();
}

void pton_dispose_arena(pton_arena_t *arena) {
  delete arena;
}

void *pton_arena_t::alloc_raw(uint32_t bytes) {
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

pton_variant_t pton_new_array(pton_arena_t *arena) {
  return arena->new_array().to_c();
}

array_t pton_arena_t::new_array(uint32_t init_capacity) {
  pton_arena_array_t *data = alloc_value<pton_arena_array_t>();
  variant_t result(pton_variant_t::rtArenaArray, new (data) pton_arena_array_t(this, init_capacity));
  return array_t(result);
}

pton_variant_t pton_new_array_with_capacity(pton_arena_t *arena, uint32_t init_capacity) {
  return arena->new_array(init_capacity).to_c();
}

map_t pton_arena_t::new_map() {
  pton_arena_map_t *data = alloc_value<pton_arena_map_t>();
  variant_t result(pton_variant_t::rtArenaMap, new (data) pton_arena_map_t(this));
  return map_t(result);
}

pton_variant_t pton_new_map(pton_arena_t *arena) {
  return arena->new_map().to_c();
}

pton_variant_t pton_new_c_str(pton_arena_t *arena, const char *str) {
  return arena->new_string(str).to_c();
}

string_t pton_arena_t::new_string(const char *str) {
  return new_string(str, strlen(str));
}

string_t pton_arena_t::new_string(const char *str, uint32_t length) {
  pton_arena_string_t *data = alloc_value<pton_arena_string_t>();
  char *own_str = alloc_values<char>(length + 1);
  memcpy(own_str, str, length);
  own_str[length] = '\0';
  variant_t result(pton_variant_t::rtArenaString, new (data) pton_arena_string_t(own_str, length, true));
  return string_t(result);
}

pton_variant_t pton_new_string(pton_arena_t *arena, const char *str, uint32_t length) {
  return arena->new_string(str, length).to_c();
}

string_t pton_arena_t::new_string(uint32_t length) {
  pton_arena_string_t *data = alloc_value<pton_arena_string_t>();
  char *own_str = alloc_values<char>(length + 1);
  memset(own_str, '\0', length + 1);
  variant_t result(pton_variant_t::rtArenaString, new (data) pton_arena_string_t(own_str, length, false));
  return string_t(result);
}

pton_variant_t pton_new_mutable_string(pton_arena_t *arena, uint32_t length) {
  return arena->new_string(length).to_c();
}

blob_t pton_arena_t::new_blob(const void *start, uint32_t size) {
  pton_arena_blob_t *data = alloc_value<pton_arena_blob_t>();
  uint8_t *own_start = alloc_values<uint8_t>(size);
  memcpy(own_start, start, size);
  variant_t result(pton_variant_t::rtArenaBlob, new (data) pton_arena_blob_t(own_start, size, true));
  return blob_t(result);
}

blob_t pton_arena_t::new_blob(uint32_t size) {
  pton_arena_blob_t *data = alloc_value<pton_arena_blob_t>();
  uint8_t *bytes = alloc_values<uint8_t>(size);
  memset(bytes, 0, size);
  variant_t result(pton_variant_t::rtArenaBlob, new (data) pton_arena_blob_t(data, size, false));
  return blob_t(result);
}

sink_t pton_arena_t::new_sink() {
  pton_sink_t *data = alloc_sink();
  return sink_t(data);
}

pton_sink_t *pton_arena_t::alloc_sink() {
  pton_sink_t *result = alloc_value<pton_sink_t>();
  return new (result) pton_sink_t(this);
}

// Creates and returns a new sink value.
pton_sink_t *pton_new_sink(pton_arena_t *arena) {
  return arena->alloc_sink();
}

pton_variant_t::type_t pton_get_type(pton_variant_t variant) {
  return static_cast<pton_variant_t::type_t>(variant.repr_tag_ >> 4);
}

pton_variant_t::type_t variant_t::type() const {
  return pton_get_type(value_);
}

bool pton_variants_equal(pton_variant_t a, pton_variant_t b) {
  pton_variant_t::type_t a_type = pton_get_type(a);
  pton_variant_t::type_t b_type = pton_get_type(b);
  if (a_type != b_type)
    return false;
  switch (a_type) {
    case pton_variant_t::vtInteger:
      return pton_get_integer_value(a) == pton_get_integer_value(b);
    case pton_variant_t::vtString: {
      uint32_t length = pton_get_string_length(a);
      if (pton_get_string_length(b) != length)
        return false;
      return strncmp(pton_get_string_chars(a), pton_get_string_chars(b), length) == 0;
    }
    case pton_variant_t::vtBlob: {
      uint32_t size = pton_get_blob_size(a);
      if (pton_get_blob_size(b) != size)
        return false;
      return strncmp(static_cast<const char*>(pton_get_blob_data(a)),
          static_cast<const char*>(pton_get_blob_data(b)), size) == 0;
    }
    case pton_variant_t::vtArray:
      return a.payload_.as_arena_array_ == b.payload_.as_arena_array_;
    case pton_variant_t::vtMap:
      return a.payload_.as_arena_map_ == b.payload_.as_arena_map_;
    case pton_variant_t::vtNull:
      return true;
    case pton_variant_t::vtBool:
      return a.repr_tag_ == b.repr_tag_;
    default:
      return false;
  }
}

bool variant_t::operator==(variant_t that) {
  return pton_variants_equal(value_, that.value_);
}

bool pton_is_frozen(pton_variant_t variant) {
  switch (variant.repr_tag_) {
    case pton_variant_t::rtInteger:
    case pton_variant_t::rtNull:
    case pton_variant_t::rtTrue:
    case pton_variant_t::rtFalse:
    case pton_variant_t::rtExternalString:
    case pton_variant_t::rtExternalBlob:
      return true;
    case pton_variant_t::rtArenaArray:
    case pton_variant_t::rtArenaMap:
    case pton_variant_t::rtArenaString:
    case pton_variant_t::rtArenaBlob:
      return variant.payload_.as_arena_value_->is_frozen();
    default:
      return false;
  }
}

bool variant_t::is_frozen() {
  return pton_is_frozen(value_);
}

void pton_ensure_frozen(pton_variant_t variant) {
  switch (variant.repr_tag_) {
    case pton_variant_t::rtArenaArray:
    case pton_variant_t::rtArenaMap:
    case pton_variant_t::rtArenaString:
    case pton_variant_t::rtArenaBlob:
      variant.payload_.as_arena_value_->ensure_frozen();
      break;
    default:
      break;
  }
}

void variant_t::ensure_frozen() {
  pton_ensure_frozen(value_);
}

variant_t variant_t::blob(const void *data, uint32_t size) {
  return variant_t(pton_blob(data, size));
}

array_t::array_t(variant_t variant) : variant_t() {
  // Initialize this to the null value and then, if the given variant is an
  // array, override with the variant's state.
  if (variant.is_array())
    *static_cast<variant_t*>(this) = variant;
}

bool pton_array_add(pton_variant_t array, pton_variant_t value) {
  return pton_is_array(array) && array.payload_.as_arena_array_->add(value);
}

bool variant_t::array_add(variant_t value) {
  return pton_array_add(value_, value.value_);
}

uint32_t pton_get_array_length(pton_variant_t variant) {
  return pton_is_array(variant) ? variant.payload_.as_arena_array_->length() : 0;
}

uint32_t variant_t::array_length() const {
  return pton_get_array_length(value_);
}

pton_variant_t pton_array_get(pton_variant_t variant, uint32_t index) {
  return pton_is_array(variant)
      ? variant.payload_.as_arena_array_->get(index).to_c()
      : pton_null();
}

variant_t variant_t::array_get(uint32_t index) const {
  return pton_array_get(value_, index);
}

pton_arena_array_t::pton_arena_array_t(pton_arena_t *origin, uint32_t init_capacity)
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

variant_t pton_arena_array_t::get(uint32_t index) {
  return (index < length_) ? elms_[index] : variant_t::null();
}

map_t::map_t(variant_t variant) : variant_t() {
  // Initialize this to the null value and then, if the given variant is a map,
  // override with the variant's state.
  if (variant.type() == pton_variant_t::vtMap)
    *static_cast<variant_t*>(this) = variant;
}

uint32_t pton_map_size(pton_variant_t variant) {
  return pton_is_map(variant) ? variant.payload_.as_arena_map_->size() : 0;
}

uint32_t variant_t::map_size() const {
  return pton_map_size(value_);
}

bool pton_map_set(pton_variant_t map, pton_variant_t key, pton_variant_t value) {
  return pton_is_map(map) && map.payload_.as_arena_map_->set(key, value);
}

bool variant_t::map_set(variant_t key, variant_t value) {
  return pton_map_set(value_, key.value_, value.value_);
}

pton_variant_t pton_map_get(pton_variant_t variant, pton_variant_t key) {
  return pton_is_map(variant)
      ? variant.payload_.as_arena_map_->get(key).to_c()
      : pton_null();
}

variant_t variant_t::map_get(variant_t key) const {
  return pton_map_get(value_, key.value_);
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

uint32_t pton_get_string_length(pton_variant_t variant) {
  switch (variant.repr_tag_) {
  case pton_variant_t::rtExternalString:
    return variant.payload_.as_external_string_.length_;
  case pton_variant_t::rtArenaString:
    return variant.payload_.as_arena_string_->length();
  default:
    return 0;
  }
}

uint32_t variant_t::string_length() const {
  return pton_get_string_length(value_);
}

const char *pton_get_string_chars(pton_variant_t variant) {
  switch (variant.repr_tag_) {
    case pton_variant_t::rtExternalString:
      return variant.payload_.as_external_string_.chars_;
    case pton_variant_t::rtArenaString:
      return variant.payload_.as_arena_string_->chars();
    default:
      return NULL;
  }
}

const char *variant_t::string_chars() const {
  return pton_get_string_chars(value_);
}

char variant_t::string_get(uint32_t index) const {
  if (index >= string_length())
    // Only real strings have nonzero length so this all non-strings end up
    // here.
    return 0;
  return string_chars()[index];
}

bool variant_t::string_set(uint32_t index, char c) {
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

pton_arena_string_t::pton_arena_string_t(char *chars, uint32_t length, bool is_frozen)
  : chars_(chars)
  , length_(length) {
  is_frozen_ = is_frozen;
}

bool pton_arena_string_t::set(uint32_t index, char c) {
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

uint32_t pton_get_blob_size(pton_variant_t variant) {
  switch (variant.repr_tag_) {
    case pton_variant_t::rtExternalBlob:
      return variant.payload_.as_external_blob_.size_;
    case pton_variant_t::rtArenaBlob:
      return variant.payload_.as_arena_blob_->size();
    default:
      return 0;
  }
}

uint32_t variant_t::blob_size() const {
  return pton_get_blob_size(value_);
}

const void *pton_get_blob_data(pton_variant_t variant) {
  switch (variant.repr_tag_) {
    case pton_variant_t::rtExternalBlob:
      return variant.payload_.as_external_blob_.data_;
    case pton_variant_t::rtArenaBlob:
      return variant.payload_.as_arena_blob_->data();
    default:
      return NULL;
  }
}

const void *variant_t::blob_data() const {
  return pton_get_blob_data(value_);
}

uint8_t variant_t::blob_get(uint32_t index) const {
  if (index >= blob_size())
    // Only real blobs have nonzero size so this all non-blobs end up here.
    return 0;
  return static_cast<const uint8_t*>(blob_data())[index];
}

bool variant_t::blob_set(uint32_t index, uint8_t b) {
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

sink_t::sink_t(pton_sink_t *data)
  : data_(data) { }

pton_variant_t pton_sink_get(pton_sink_t *sink) {
  return sink->operator*().to_c();
}

variant_t sink_t::operator*() const {
  return pton_sink_get(data_);
}

bool pton_sink_set(pton_sink_t *sink, pton_variant_t value) {
  return sink->set(value);
}

bool sink_t::set(variant_t value) {
  return pton_sink_set(data_, value.value_);
}

pton_sink_t::pton_sink_t(pton_arena_t *origin)
  : is_empty_(true)
  , origin_(origin) { }

variant_t pton_sink_t::operator*() {
  return value_;
}

bool pton_sink_t::set(variant_t value) {
  if (is_empty_) {
    is_empty_ = false;
    value_ = value;
    return true;
  } else {
    return false;
  }
}
