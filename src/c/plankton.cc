//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "plankton-inl.hh"
#include "stdc.h"
#include "callback.hh"
#include <new>

using namespace plankton;

// The current plankton version.
#define BINARY_VERSION 0xBE

// Expands to an initializer for a variant with the given tag and length fields
// in their headers. In particular, this initializes the version field
// appropriately.
#define VARIANT_INIT(tag, length) {{tag, BINARY_VERSION, length}, {0}}

typedef pton_variant_t::pton_variant_header_t header_t;

// Shared between all the arena types.
struct pton_arena_value_t {
public:
  pton_arena_value_t() : is_frozen_(false) { }

  bool is_frozen() { return is_frozen_; }

  void ensure_frozen() { is_frozen_ = true; }

protected:
  bool is_frozen_;
};

// An arena-allocated array.
struct pton_arena_array_t : public pton_arena_value_t {
public:
  pton_arena_array_t(pton_arena_t *origin, uint32_t init_capacity);

  bool add(variant_t value);

  pton_sink_t *add_sink();

  variant_t get(uint32_t index);

  uint32_t length() { return length_; }

  bool set_from_sink(size_t index, variant_t value);

private:
  friend struct pton_arena_t;
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

  bool set(pton_sink_t **key_out, pton_sink_t **value_out);

  variant_t get(variant_t key) const;

  uint32_t size() const { return size_; }

  bool set_key_from_sink(size_t index, variant_t value);

  bool set_value_from_sink(size_t index, variant_t value);

private:
  friend class ::map_iterator_t;

  pton_arena_t *origin_;
  uint32_t size_;
  uint32_t capacity_;
  entry_t *elms_;
};

struct pton_arena_string_t : public pton_arena_value_t {
public:
  pton_arena_string_t(char *chars, uint32_t length, variant_t encoding, bool is_frozen);

  uint32_t length() { return length_; }

  const char *chars() { return chars_; }

  variant_t encoding() { return encoding_; }

private:
  char *chars_;
  uint32_t length_;
  variant_t encoding_;
};

struct pton_arena_blob_t : public pton_arena_value_t {
public:
  pton_arena_blob_t(void *data, uint32_t size, bool is_frozen);

  uint32_t size() { return size_; }

  void *data() { return data_; }

private:
  void *data_;
  uint32_t size_;
};

// Generic interface for arena-allocated values that have a virtual destructor
// that must be called on cleanup.
class plankton::disposable_t {
public:
  virtual ~disposable_t() { };
};

struct pton_sink_t : public disposable_t {
public:
  explicit pton_sink_t(pton_arena_t *origin, sink_set_callback_t on_set);

  // Sets this sink's value but only if the on_set callback returns true and
  // the value hasn't already been set.
  bool set(variant_t value);

  bool set_string(const char *chars, uint32_t length);

  variant_t as_array();

  variant_t as_map();

  sink_t new_sink(variant_t *out);

private:
  bool is_empty_;
  pton_arena_t *origin_;
  sink_set_callback_t on_set_;
};

pton_arena_t *pton_new_arena() {
  return new arena_t();
}

void pton_dispose_arena(pton_arena_t *arena) {
  delete arena;
}

void *pton_arena_t::alloc_raw(uint32_t bytes) {
  uint8_t *result = new uint8_t[bytes];
  blocks_.push_back(result);
  return result;
}

void pton_arena_t::add_disposable(plankton::disposable_t *ptr) {
  disposables_.push_back(ptr);
}

pton_arena_t::~pton_arena_t() {
  for (size_t i = 0; i < disposables_.size(); i++) {
    disposable_t *ptr = disposables_[i];
    ptr->~disposable_t();
  }
  for (size_t i = 0; i < blocks_.size(); i++)
    delete[] blocks_[i];
}

array_t pton_arena_t::new_array() {
  return new_array(pton_arena_array_t::kDefaultInitCapacity);
}

pton_variant_t pton_new_array(pton_arena_t *arena) {
  return arena->new_array().to_c();
}

variant_t::variant_t(repr_tag_t tag, pton_arena_value_t *arena_value) {
  pton_variant_t value = VARIANT_INIT(tag, 0);
  value.payload_.as_arena_value_ = arena_value;
  value_ = value;
}

array_t pton_arena_t::new_array(uint32_t init_capacity) {
  pton_arena_array_t *data = alloc_value<pton_arena_array_t>();
  variant_t result(header_t::PTON_REPR_ARNA_ARRAY, new (data) pton_arena_array_t(this, init_capacity));
  return array_t(result);
}

pton_variant_t pton_new_array_with_capacity(pton_arena_t *arena, uint32_t init_capacity) {
  return arena->new_array(init_capacity).to_c();
}

map_t pton_arena_t::new_map() {
  pton_arena_map_t *data = alloc_value<pton_arena_map_t>();
  variant_t result(header_t::PTON_REPR_ARNA_MAP, new (data) pton_arena_map_t(this));
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
  return new_string(str, length, variant_t::default_string_encoding());
}

string_t pton_arena_t::new_string(const void *str, uint32_t length,
    variant_t encoding) {
  pton_arena_string_t *data = alloc_value<pton_arena_string_t>();
  char *own_str = alloc_values<char>(length + 1);
  memcpy(own_str, str, length);
  own_str[length] = '\0';
  variant_t result(header_t::PTON_REPR_ARNA_STRING, new (data) pton_arena_string_t(
      own_str, length, encoding, true));
  return string_t(result);
}

pton_variant_t pton_new_string(pton_arena_t *arena, const char *str, uint32_t length) {
  return arena->new_string(str, length).to_c();
}

string_t pton_arena_t::new_string(uint32_t length) {
  return new_string(length, variant_t::default_string_encoding());
}

string_t pton_arena_t::new_string(uint32_t length, variant_t encoding) {
  pton_arena_string_t *data = alloc_value<pton_arena_string_t>();
  char *own_str = alloc_values<char>(length + 1);
  memset(own_str, '\0', length + 1);
  variant_t result(header_t::PTON_REPR_ARNA_STRING, new (data) pton_arena_string_t(
      own_str, length, encoding, false));
  return string_t(result);
}

pton_variant_t pton_new_mutable_string(pton_arena_t *arena, uint32_t length) {
  return arena->new_string(length).to_c();
}

blob_t pton_arena_t::new_blob(const void *start, uint32_t size) {
  pton_arena_blob_t *data = alloc_value<pton_arena_blob_t>();
  uint8_t *own_start = alloc_values<uint8_t>(size);
  memcpy(own_start, start, size);
  variant_t result(header_t::PTON_REPR_ARNA_BLOB, new (data) pton_arena_blob_t(own_start, size, true));
  return blob_t(result);
}

blob_t pton_arena_t::new_blob(uint32_t size) {
  pton_arena_blob_t *data = alloc_value<pton_arena_blob_t>();
  uint8_t *bytes = alloc_values<uint8_t>(size);
  memset(bytes, 0, size);
  variant_t result(header_t::PTON_REPR_ARNA_BLOB, new (data) pton_arena_blob_t(data, size, false));
  return blob_t(result);
}

static bool set_variant_ptr(variant_t *out, variant_t value) {
  *out = value;
  return true;
}

sink_t pton_arena_t::new_sink(variant_t *out) {
  pton_sink_t *data = alloc_sink(tclib::new_callback(set_variant_ptr, out));
  return sink_t(data);
}

pton_sink_t *pton_arena_t::alloc_sink(plankton::sink_set_callback_t on_set) {
  pton_sink_t *result = alloc_value<pton_sink_t>();
  if (!on_set.is_empty())
    // If there is a nontrivial callback we need to be sure it gets disposed on
    // arena teardown.
    add_disposable(result);
  return new (result) pton_sink_t(this, on_set);
}

// Creates and returns a new sink value.
pton_sink_t *pton_new_sink(pton_arena_t *arena) {
  return arena->alloc_sink(tclib::empty_callback());
}

static void pton_check_binary_version(pton_variant_t variant) {
  if (variant.header_.binary_version_ !=BINARY_VERSION) {
    fprintf(stderr, "Plankton version mismatch: expected %i, found %i.\n",
        BINARY_VERSION, variant.header_.binary_version_);
    fflush(stderr);
    abort();
  }
}

pton_type_t pton_type(pton_variant_t variant) {
  pton_check_binary_version(variant);
  return static_cast<pton_type_t>(variant.header_.repr_tag_ >> 4);
}

pton_type_t variant_t::type() const {
  return pton_type(value_);
}

bool pton_variants_equal(pton_variant_t a, pton_variant_t b) {
  pton_check_binary_version(a);
  pton_check_binary_version(b);
  pton_type_t a_type = pton_type(a);
  pton_type_t b_type = pton_type(b);
  if (a_type != b_type)
    return false;
  switch (a_type) {
    case PTON_INTEGER:
      return pton_int64_value(a) == pton_int64_value(b);
    case PTON_STRING: {
      uint32_t length = pton_string_length(a);
      if (pton_string_length(b) != length)
        return false;
      return strncmp(pton_string_chars(a), pton_string_chars(b), length) == 0;
    }
    case PTON_BLOB: {
      uint32_t size = pton_blob_size(a);
      if (pton_blob_size(b) != size)
        return false;
      return strncmp(static_cast<const char*>(pton_blob_data(a)),
          static_cast<const char*>(pton_blob_data(b)), size) == 0;
    }
    case PTON_ARRAY:
      return a.payload_.as_arena_array_ == b.payload_.as_arena_array_;
    case PTON_MAP:
      return a.payload_.as_arena_map_ == b.payload_.as_arena_map_;
    case PTON_NULL:
      return true;
    case PTON_BOOL:
      return a.header_.repr_tag_ == b.header_.repr_tag_;
    case PTON_ID:
      return a.header_.length_ == b.header_.length_
          && a.payload_.as_inline_id_ == b.payload_.as_inline_id_;
    default:
      return false;
  }
}

bool variant_t::operator==(variant_t that) {
  return pton_variants_equal(value_, that.value_);
}

bool pton_is_frozen(pton_variant_t variant) {
  pton_check_binary_version(variant);
  switch (variant.header_.repr_tag_) {
    case header_t::PTON_REPR_INT64:
    case header_t::PTON_REPR_NULL:
    case header_t::PTON_REPR_TRUE:
    case header_t::PTON_REPR_FALSE:
    case header_t::PTON_REPR_EXTN_STRING:
    case header_t::PTON_REPR_EXTN_BLOB:
    case header_t::PTON_REPR_INLN_ID:
      return true;
    case header_t::PTON_REPR_ARNA_ARRAY:
    case header_t::PTON_REPR_ARNA_MAP:
    case header_t::PTON_REPR_ARNA_STRING:
    case header_t::PTON_REPR_ARNA_BLOB:
      return variant.payload_.as_arena_value_->is_frozen();
    default:
      return false;
  }
}

bool variant_t::is_frozen() {
  return pton_is_frozen(value_);
}

void pton_ensure_frozen(pton_variant_t variant) {
  pton_check_binary_version(variant);
  switch (variant.header_.repr_tag_) {
    case header_t::PTON_REPR_ARNA_ARRAY:
    case header_t::PTON_REPR_ARNA_MAP:
    case header_t::PTON_REPR_ARNA_STRING:
    case header_t::PTON_REPR_ARNA_BLOB:
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

variant_t variant_t::default_string_encoding() {
  return variant_t("utf-8");
}

array_t::array_t(variant_t variant) : variant_t() {
  // Initialize this to the null value and then, if the given variant is an
  // array, override with the variant's state.
  if (variant.is_array())
    *static_cast<variant_t*>(this) = variant;
}

bool pton_array_add(pton_variant_t array, pton_variant_t value) {
  pton_check_binary_version(array);
  pton_check_binary_version(value);
  return pton_is_array(array) && array.payload_.as_arena_array_->add(value);
}

bool variant_t::array_add(variant_t value) {
  return pton_array_add(value_, value.value_);
}

pton_sink_t *pton_array_add_sink(pton_variant_t array) {
  pton_check_binary_version(array);
  if (!pton_is_array(array))
    return NULL;
  return array.payload_.as_arena_array_->add_sink();
}

sink_t variant_t::array_add_sink() {
  return sink_t(pton_array_add_sink(value_));
}

sink_t array_t::add() {
  return array_add_sink();
}

uint32_t pton_array_length(pton_variant_t variant) {
  pton_check_binary_version(variant);
  return pton_is_array(variant) ? variant.payload_.as_arena_array_->length() : 0;
}

uint32_t variant_t::array_length() const {
  return pton_array_length(value_);
}

pton_variant_t pton_array_get(pton_variant_t variant, uint32_t index) {
  pton_check_binary_version(variant);
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

bool pton_arena_array_t::set_from_sink(size_t index, variant_t value) {
  if (is_frozen())
    return false;
  elms_[index] = value;
  return true;
}

pton_sink_t *pton_arena_array_t::add_sink() {
  size_t index = length_;
  if (!add(variant_t::null()))
      return false;
  sink_set_callback_t on_set = tclib::new_callback(
      &pton_arena_array_t::set_from_sink, this, index);
  return origin_->alloc_sink(on_set);
}

variant_t pton_arena_array_t::get(uint32_t index) {
  return (index < length_) ? elms_[index] : variant_t::null();
}

map_t::map_t(variant_t variant) : variant_t() {
  // Initialize this to the null value and then, if the given variant is a map,
  // override with the variant's state.
  if (variant.type() == PTON_MAP)
    *static_cast<variant_t*>(this) = variant;
}

uint32_t pton_map_size(pton_variant_t variant) {
  pton_check_binary_version(variant);
  return pton_is_map(variant) ? variant.payload_.as_arena_map_->size() : 0;
}

uint32_t variant_t::map_size() const {
  return pton_map_size(value_);
}

bool pton_map_set(pton_variant_t map, pton_variant_t key, pton_variant_t value) {
  pton_check_binary_version(map);
  pton_check_binary_version(key);
  pton_check_binary_version(value);
  return pton_is_map(map) && map.payload_.as_arena_map_->set(key, value);
}

bool variant_t::map_set(variant_t key, variant_t value) {
  return pton_map_set(value_, key.value_, value.value_);
}

pton_variant_t pton_map_get(pton_variant_t variant, pton_variant_t key) {
  pton_check_binary_version(variant);
  pton_check_binary_version(key);
  return pton_is_map(variant)
      ? variant.payload_.as_arena_map_->get(key).to_c()
      : pton_null();
}

bool pton_map_set_sinks(pton_variant_t map, pton_sink_t **key_out,
    pton_sink_t **value_out) {
  pton_check_binary_version(map);
  return pton_is_map(map) && map.payload_.as_arena_map_->set(key_out, value_out);
}

bool variant_t::map_set(sink_t *key_out, sink_t *value_out) {
  pton_sink_t *key_ptr_out = NULL;
  pton_sink_t *value_ptr_out = NULL;
  if (pton_map_set_sinks(value_, &key_ptr_out, &value_ptr_out)) {
    *key_out = sink_t(key_ptr_out);
    *value_out = sink_t(value_ptr_out);
    return true;
  } else {
    return false;
  }
}

variant_t variant_t::map_get(variant_t key) const {
  return pton_map_get(value_, key.value_);
}

uint64_t pton_id64_value(pton_variant_t variant) {
  pton_check_binary_version(variant);
  return pton_is_id(variant)
      ? variant.payload_.as_inline_id_
      : 0;
}

uint64_t variant_t::id64_value() const {
  return pton_id64_value(value_);
}

uint32_t pton_id_size(pton_variant_t variant) {
  pton_check_binary_version(variant);
  return pton_is_id(variant)
      ? variant.header_.length_
      : 0;
}

uint32_t variant_t::id_size() const {
  return pton_id_size(value_);
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

bool pton_arena_map_t::set_key_from_sink(size_t index, variant_t value) {
  if (is_frozen())
    return false;
  elms_[index].key = value;
  return true;
}

bool pton_arena_map_t::set_value_from_sink(size_t index, variant_t value) {
  if (is_frozen())
    return false;
  elms_[index].value = value;
  return true;
}

bool pton_arena_map_t::set(pton_sink_t **key_out, pton_sink_t **value_out) {
  size_t index = size_;
  if (!(set(variant_t::null(), variant_t::null())))
    return false;

  pton_sink_t *key = origin_->alloc_sink(
      tclib::new_callback(&pton_arena_map_t::set_key_from_sink, this, index));
  *key_out = key;
  pton_sink_t *value = origin_->alloc_sink(
      tclib::new_callback(&pton_arena_map_t::set_value_from_sink, this, index));
  *value_out = value;
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

uint32_t pton_string_length(pton_variant_t variant) {
  pton_check_binary_version(variant);
  switch (variant.header_.repr_tag_) {
  case header_t::PTON_REPR_EXTN_STRING:
    return variant.header_.length_;
  case header_t::PTON_REPR_ARNA_STRING:
    return variant.payload_.as_arena_string_->length();
  default:
    return 0;
  }
}

uint32_t variant_t::string_length() const {
  return pton_string_length(value_);
}

const char *pton_string_chars(pton_variant_t variant) {
  pton_check_binary_version(variant);
  switch (variant.header_.repr_tag_) {
    case header_t::PTON_REPR_EXTN_STRING:
      return variant.payload_.as_external_string_chars_;
    case header_t::PTON_REPR_ARNA_STRING:
      return variant.payload_.as_arena_string_->chars();
    default:
      return NULL;
  }
}

char *pton_string_mutable_chars(pton_variant_t variant) {
  return pton_is_frozen(variant)
      ? NULL
      : const_cast<char*>(pton_string_chars(variant));
}

variant_t variant_t::string_encoding() const {
  return pton_string_encoding(value_);
}

pton_variant_t pton_string_encoding(pton_variant_t variant) {
  pton_check_binary_version(variant);
  switch (variant.header_.repr_tag_) {
    case header_t::PTON_REPR_EXTN_STRING:
      return variant_t::default_string_encoding().to_c();
    case header_t::PTON_REPR_ARNA_STRING:
      return variant.payload_.as_arena_string_->encoding().to_c();
    default:
      return pton_null();
  }
}

const char *variant_t::string_chars() const {
  return pton_string_chars(value_);
}

char *variant_t::string_mutable_chars() const {
  return pton_string_mutable_chars(value_);
}

string_t::string_t(variant_t variant) {
  if (variant.is_string())
    *static_cast<variant_t*>(this) = variant;
}

pton_arena_string_t::pton_arena_string_t(char *chars, uint32_t length,
    variant_t encoding, bool is_frozen)
  : chars_(chars)
  , length_(length)
  , encoding_(encoding) {
  is_frozen_ = is_frozen;
}

pton_arena_blob_t::pton_arena_blob_t(void *data, uint32_t size, bool is_frozen)
  : data_(data)
  , size_(size) {
  is_frozen_ = is_frozen;
}

uint32_t pton_blob_size(pton_variant_t variant) {
  pton_check_binary_version(variant);
  switch (variant.header_.repr_tag_) {
    case header_t::PTON_REPR_EXTN_BLOB:
      return variant.header_.length_;
    case header_t::PTON_REPR_ARNA_BLOB:
      return variant.payload_.as_arena_blob_->size();
    default:
      return 0;
  }
}

uint32_t variant_t::blob_size() const {
  return pton_blob_size(value_);
}

const void *pton_blob_data(pton_variant_t variant) {
  pton_check_binary_version(variant);
  switch (variant.header_.repr_tag_) {
    case header_t::PTON_REPR_EXTN_BLOB:
      return variant.payload_.as_external_blob_data_;
    case header_t::PTON_REPR_ARNA_BLOB:
      return variant.payload_.as_arena_blob_->data();
    default:
      return NULL;
  }
}

const void *variant_t::blob_data() const {
  return pton_blob_data(value_);
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

bool pton_sink_set(pton_sink_t *sink, pton_variant_t value) {
  pton_check_binary_version(value);
  return sink->set(value);
}

bool sink_t::set(variant_t value) {
  return pton_sink_set(data_, value.value_);
}

pton_variant_t pton_sink_as_array(pton_sink_t *sink) {
  return sink->as_array().to_c();
}

array_t sink_t::as_array() {
  return array_t(pton_sink_as_array(data_));
}

pton_variant_t pton_sink_as_map(pton_sink_t *sink) {
  return sink->as_map().to_c();
}

map_t sink_t::as_map() {
  return map_t(pton_sink_as_map(data_));
}

pton_sink_t *pton_sink_new_sink(pton_sink_t *sink, pton_variant_t *out) {
  return sink_t(sink).new_sink(reinterpret_cast<variant_t*>(out)).to_c();
}

sink_t sink_t::new_sink(variant_t *out) {
  return to_c()->new_sink(out);
}

sink_t pton_sink_t::new_sink(variant_t *out) {
  return origin_->new_sink(out);
}

bool pton_sink_set_string(pton_sink_t *sink, const char *chars, uint32_t length) {
  return sink->set_string(chars, length);
}

bool sink_t::set_string(const char *chars, uint32_t length) {
  return pton_sink_set_string(data_, chars, length);
}

pton_sink_t::pton_sink_t(pton_arena_t *origin, sink_set_callback_t on_set)
  : is_empty_(true)
  , origin_(origin)
  , on_set_(on_set) { }

bool pton_sink_t::set(variant_t value) {
  if (is_empty_) {
    if (!on_set_.is_empty() && !on_set_(value))
      return false;
    is_empty_ = false;
    return true;
  } else {
    return false;
  }
}

bool pton_sink_t::set_string(const char *chars, uint32_t length) {
  if (!is_empty_)
    return false;
  variant_t value = origin_->new_string(chars, length);
  return set(value);
}

variant_t pton_sink_t::as_array() {
  if (!is_empty_)
    return variant_t::null();
  variant_t value = origin_->new_array();
  return set(value) ? value : variant_t::null();
}

variant_t pton_sink_t::as_map() {
  if (!is_empty_)
    return variant_t::null();
  variant_t value = origin_->new_map();
  return set(value) ? value : variant_t::null();
}

bool pton_is_integer(pton_variant_t variant) {
  pton_check_binary_version(variant);
  return variant.header_.repr_tag_ == header_t::PTON_REPR_INT64;
}

bool pton_is_array(pton_variant_t variant) {
  pton_check_binary_version(variant);
  return variant.header_.repr_tag_ == header_t::PTON_REPR_ARNA_ARRAY;
}

bool pton_is_map(pton_variant_t variant) {
  pton_check_binary_version(variant);
  return variant.header_.repr_tag_ == header_t::PTON_REPR_ARNA_MAP;
}

bool pton_is_id(pton_variant_t variant) {
  pton_check_binary_version(variant);
  return variant.header_.repr_tag_ == header_t::PTON_REPR_INLN_ID;
}

bool pton_bool_value(pton_variant_t variant) {
  pton_check_binary_version(variant);
  return variant.header_.repr_tag_ == header_t::PTON_REPR_TRUE;
}

int64_t pton_int64_value(pton_variant_t variant) {
  pton_check_binary_version(variant);
  return pton_is_integer(variant) ? variant.payload_.as_int64_ : 0;
}

pton_variant_t pton_null() {
  pton_variant_t result = VARIANT_INIT(header_t::PTON_REPR_NULL, 0);
  return result;
}

pton_variant_t pton_true() {
  pton_variant_t result = VARIANT_INIT(header_t::PTON_REPR_TRUE, 0);
  return result;
}

pton_variant_t pton_false() {
  pton_variant_t result = VARIANT_INIT(header_t::PTON_REPR_FALSE, 0);
  return result;
}

pton_variant_t pton_bool(bool value) {
  pton_variant_t result = VARIANT_INIT(
      value ? header_t::PTON_REPR_TRUE : header_t::PTON_REPR_FALSE,
      0);
  return result;
}

pton_variant_t pton_integer(int64_t value) {
  pton_variant_t result = VARIANT_INIT(header_t::PTON_REPR_INT64, 0);
  result.payload_.as_int64_ = value;
  return result;
}

pton_variant_t pton_string(const char *chars, uint32_t length) {
  pton_variant_t result = VARIANT_INIT(header_t::PTON_REPR_EXTN_STRING,
      length);
  result.payload_.as_external_string_chars_ = chars;
  return result;
}

pton_variant_t pton_c_str(const char *chars) {
  return pton_string(chars, strlen(chars));
}

pton_variant_t pton_blob(const void *data, uint32_t size) {
  pton_variant_t result = VARIANT_INIT(header_t::PTON_REPR_EXTN_BLOB,
      size);
  result.payload_.as_external_blob_data_ = data;
  return result;
}

pton_variant_t pton_id64(uint64_t value) {
  pton_variant_t result = VARIANT_INIT(header_t::PTON_REPR_INLN_ID, 64);
  result.payload_.as_inline_id_ = value;
  return result;
}

pton_variant_t pton_id32(uint32_t value) {
  pton_variant_t result = VARIANT_INIT(header_t::PTON_REPR_INLN_ID, 32);
  result.payload_.as_inline_id_ = value;
  return result;
}

pton_variant_t pton_id(uint32_t size, uint64_t value) {
  pton_variant_t result = VARIANT_INIT(header_t::PTON_REPR_INLN_ID, size);
  result.payload_.as_inline_id_ = value;
  return result;
}
