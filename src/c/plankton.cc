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
  pton_arena_array_t(Arena *origin, uint32_t init_capacity);

  bool add(Variant value);

  pton_sink_t *add_sink();

  bool set_from_sink(size_t index, Variant value);

private:
  friend class plankton::Variant;
  friend class plankton::Arena;
  static const uint32_t kDefaultInitCapacity = 8;
  Arena *origin_;
  uint32_t length_;
  uint32_t capacity_;
  Variant *elms_;
};

struct pton_arena_map_t : public pton_arena_value_t {
public:
  struct entry_t {
    Variant key;
    Variant value;
  };

  explicit pton_arena_map_t(Arena *origin);

  bool set(Variant key, Variant value);

  bool set(pton_sink_t **key_out, pton_sink_t **value_out);

  Variant get(Variant key) const;

  uint32_t size() const { return size_; }

  bool set_key_from_sink(size_t index, Variant value);

  bool set_value_from_sink(size_t index, Variant value);

private:
  friend class ::Map_Iterator;

  Arena *origin_;
  uint32_t size_;
  uint32_t capacity_;
  entry_t *elms_;
};

struct pton_arena_string_t : public pton_arena_value_t {
public:
  pton_arena_string_t(char *chars, uint32_t length, Variant encoding, bool is_frozen);

  uint32_t length() { return length_; }

  const char *chars() { return chars_; }

  Variant encoding() { return encoding_; }

private:
  char *chars_;
  uint32_t length_;
  Variant encoding_;
};

struct pton_arena_blob_t : public pton_arena_value_t {
public:
  pton_arena_blob_t(void *data, uint32_t size, bool is_frozen);

private:
  friend class plankton::Variant;
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
  explicit pton_sink_t(Arena *origin, sink_set_callback_t on_set);

  // Sets this sink's value but only if the on_set callback returns true and
  // the value hasn't already been set.
  bool set(Variant value);

private:
  friend class plankton::Sink;
  bool is_empty_;
  Arena *origin_;
  sink_set_callback_t on_set_;
};

pton_arena_t *pton_new_arena() {
  return new Arena();
}

void pton_dispose_arena(pton_arena_t *arena) {
  delete arena;
}

void *Arena::alloc_raw(uint32_t bytes) {
  uint8_t *result = new uint8_t[bytes];
  blocks_.push_back(result);
  return result;
}

void Arena::add_disposable(disposable_t *ptr) {
  disposables_.push_back(ptr);
}

Arena::~Arena() {
  for (size_t i = 0; i < disposables_.size(); i++) {
    disposable_t *ptr = disposables_[i];
    ptr->~disposable_t();
  }
  for (size_t i = 0; i < blocks_.size(); i++)
    delete[] blocks_[i];
}

Array Arena::new_array() {
  return new_array(pton_arena_array_t::kDefaultInitCapacity);
}

pton_variant_t pton_new_array(pton_arena_t *arena) {
  return Arena::from_c(arena)->new_array().to_c();
}

Variant::Variant(repr_tag_t tag, pton_arena_value_t *arena_value) {
  pton_variant_t value = VARIANT_INIT(tag, 0);
  value.payload_.as_arena_value_ = arena_value;
  value_ = value;
}

Array Arena::new_array(uint32_t init_capacity) {
  pton_arena_array_t *data = alloc_value<pton_arena_array_t>();
  Variant result(header_t::PTON_REPR_ARNA_ARRAY, new (data) pton_arena_array_t(this, init_capacity));
  return Array(result);
}

pton_variant_t pton_new_array_with_capacity(pton_arena_t *arena, uint32_t init_capacity) {
  return Arena::from_c(arena)->new_array(init_capacity).to_c();
}

Map Arena::new_map() {
  pton_arena_map_t *data = alloc_value<pton_arena_map_t>();
  Variant result(header_t::PTON_REPR_ARNA_MAP, new (data) pton_arena_map_t(this));
  return Map(result);
}

pton_variant_t pton_new_map(pton_arena_t *arena) {
  return Arena::from_c(arena)->new_map().to_c();
}

pton_variant_t pton_new_c_str(pton_arena_t *arena, const char *str) {
  return Arena::from_c(arena)->new_string(str).to_c();
}

String Arena::new_string(const char *str) {
  return new_string(str, strlen(str));
}

String Arena::new_string(const char *str, uint32_t length) {
  return new_string(str, length, Variant::default_string_encoding());
}

String Arena::new_string(const void *str, uint32_t length,
    Variant encoding) {
  pton_arena_string_t *data = alloc_value<pton_arena_string_t>();
  char *own_str = alloc_values<char>(length + 1);
  memcpy(own_str, str, length);
  own_str[length] = '\0';
  Variant result(header_t::PTON_REPR_ARNA_STRING, new (data) pton_arena_string_t(
      own_str, length, encoding, true));
  return String(result);
}

pton_variant_t pton_new_string(pton_arena_t *arena, const char *str, uint32_t length) {
  return Arena::from_c(arena)->new_string(str, length).to_c();
}

String Arena::new_string(uint32_t length) {
  return new_string(length, Variant::default_string_encoding());
}

String Arena::new_string(uint32_t length, Variant encoding) {
  pton_arena_string_t *data = alloc_value<pton_arena_string_t>();
  char *own_str = alloc_values<char>(length + 1);
  memset(own_str, '\0', length + 1);
  Variant result(header_t::PTON_REPR_ARNA_STRING, new (data) pton_arena_string_t(
      own_str, length, encoding, false));
  return String(result);
}

pton_variant_t pton_new_mutable_string(pton_arena_t *arena, uint32_t length) {
  return Arena::from_c(arena)->new_string(length).to_c();
}

Blob Arena::new_blob(const void *start, uint32_t size) {
  pton_arena_blob_t *data = alloc_value<pton_arena_blob_t>();
  uint8_t *own_start = alloc_values<uint8_t>(size);
  memcpy(own_start, start, size);
  Variant result(header_t::PTON_REPR_ARNA_BLOB, new (data) pton_arena_blob_t(own_start, size, true));
  return Blob(result);
}

Blob Arena::new_blob(uint32_t size) {
  pton_arena_blob_t *data = alloc_value<pton_arena_blob_t>();
  uint8_t *bytes = alloc_values<uint8_t>(size);
  memset(bytes, 0, size);
  Variant result(header_t::PTON_REPR_ARNA_BLOB, new (data) pton_arena_blob_t(data, size, false));
  return Blob(result);
}

static bool set_variant_ptr(Variant *out, Variant value) {
  *out = value;
  return true;
}

Sink Arena::new_sink(Variant *out) {
  pton_sink_t *data = alloc_sink(tclib::new_callback(set_variant_ptr, out));
  return Sink(data);
}

pton_sink_t *Arena::alloc_sink(plankton::sink_set_callback_t on_set) {
  pton_sink_t *result = alloc_value<pton_sink_t>();
  if (!on_set.is_empty())
    // If there is a nontrivial callback we need to be sure it gets disposed on
    // arena teardown.
    add_disposable(result);
  return new (result) pton_sink_t(this, on_set);
}

// Creates and returns a new sink value.
pton_sink_t *pton_new_sink(pton_arena_t *arena, pton_variant_t *out) {
  return Arena::from_c(arena)->new_sink(reinterpret_cast<Variant*>(out)).to_c();
}

static void pton_check_binary_version(pton_variant_t variant) {
  if (variant.header_.binary_version_ != BINARY_VERSION) {
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

pton_type_t Variant::type() const {
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

bool Variant::operator==(Variant that) {
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

bool Variant::is_frozen() const {
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

void Variant::ensure_frozen() {
  pton_ensure_frozen(value_);
}

Variant Variant::blob(const void *data, uint32_t size) {
  return Variant(pton_blob(data, size));
}

Variant Variant::default_string_encoding() {
  return Variant("utf-8");
}

Array::Array(Variant variant) : Variant() {
  // Initialize this to the null value and then, if the given variant is an
  // array, override with the variant's state.
  if (variant.is_array())
    *static_cast<Variant*>(this) = variant;
}

bool pton_array_add(pton_variant_t array, pton_variant_t value) {
  return Variant(array).array_add(Variant(value));
}

bool Variant::array_add(Variant value) {
  pton_check_binary_version(value_);
  pton_check_binary_version(value.value_);
  if (!is_array())
    return false;
  return value_.payload_.as_arena_array_->add(value);
}

pton_sink_t *pton_array_add_sink(pton_variant_t array) {
  pton_check_binary_version(array);
  if (!pton_is_array(array))
    return NULL;
  return array.payload_.as_arena_array_->add_sink();
}

Sink Variant::array_add_sink() {
  return Sink(pton_array_add_sink(value_));
}

Sink Array::add() {
  return array_add_sink();
}

uint32_t pton_array_length(pton_variant_t variant) {
  return Variant(variant).array_length();
}

uint32_t Variant::array_length() const {
  pton_check_binary_version(value_);
  return is_array() ? value_.payload_.as_arena_array_->length_ : 0;
}

pton_variant_t pton_array_get(pton_variant_t variant, uint32_t index) {
  return Variant(variant).array_get(index).to_c();
}

Variant Variant::array_get(uint32_t index) const {
  pton_check_binary_version(value_);
  if (!is_array())
    return null();
  pton_arena_array_t *data = value_.payload_.as_arena_array_;
  return (index < data->length_) ? data->elms_[index] : null();
}

pton_arena_array_t::pton_arena_array_t(Arena *origin, uint32_t init_capacity)
  : origin_(origin)
  , length_(0)
  , capacity_(0)
  , elms_(NULL) {
  if (init_capacity < kDefaultInitCapacity)
    init_capacity = kDefaultInitCapacity;
  capacity_ = init_capacity;
  elms_ = origin->alloc_values<Variant>(capacity_);
}

bool pton_arena_array_t::add(Variant value) {
  if (is_frozen())
    return false;
  if (length_ == capacity_) {
    capacity_ *= 2;
    Variant *new_elms = origin_->alloc_values<Variant>(capacity_);
    memcpy(new_elms, elms_, sizeof(Variant) * length_);
    elms_ = new_elms;
  }
  elms_[length_++] = value;
  return true;
}

bool pton_arena_array_t::set_from_sink(size_t index, Variant value) {
  if (is_frozen())
    return false;
  elms_[index] = value;
  return true;
}

pton_sink_t *pton_arena_array_t::add_sink() {
  size_t index = length_;
  if (!add(Variant::null()))
      return false;
  sink_set_callback_t on_set = tclib::new_callback(
      &pton_arena_array_t::set_from_sink, this, index);
  return origin_->alloc_sink(on_set);
}

Map::Map(Variant variant) : Variant() {
  // Initialize this to the null value and then, if the given variant is a map,
  // override with the variant's state.
  if (variant.type() == PTON_MAP)
    *static_cast<Variant*>(this) = variant;
}

uint32_t pton_map_size(pton_variant_t variant) {
  pton_check_binary_version(variant);
  return pton_is_map(variant) ? variant.payload_.as_arena_map_->size() : 0;
}

uint32_t Variant::map_size() const {
  return pton_map_size(value_);
}

bool pton_map_set(pton_variant_t map, pton_variant_t key, pton_variant_t value) {
  pton_check_binary_version(map);
  pton_check_binary_version(key);
  pton_check_binary_version(value);
  return pton_is_map(map) && map.payload_.as_arena_map_->set(key, value);
}

bool Variant::map_set(Variant key, Variant value) {
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

bool Variant::map_set(Sink *key_out, Sink *value_out) {
  pton_sink_t *key_ptr_out = NULL;
  pton_sink_t *value_ptr_out = NULL;
  if (pton_map_set_sinks(value_, &key_ptr_out, &value_ptr_out)) {
    *key_out = Sink(key_ptr_out);
    *value_out = Sink(value_ptr_out);
    return true;
  } else {
    return false;
  }
}

Variant Variant::map_get(Variant key) const {
  return pton_map_get(value_, key.value_);
}

uint64_t pton_id64_value(pton_variant_t variant) {
  pton_check_binary_version(variant);
  return pton_is_id(variant)
      ? variant.payload_.as_inline_id_
      : 0;
}

uint64_t Variant::id64_value() const {
  return pton_id64_value(value_);
}

uint32_t pton_id_size(pton_variant_t variant) {
  pton_check_binary_version(variant);
  return pton_is_id(variant)
      ? variant.header_.length_
      : 0;
}

uint32_t Variant::id_size() const {
  return pton_id_size(value_);
}

Map_Iterator Variant::map_iter() const {
  return is_map() ? Map_Iterator(payload()->as_arena_map_) : Map_Iterator();
}

Map_Iterator::Map_Iterator(pton_arena_map_t *data)
  : data_(data)
  , cursor_(0)
  , limit_(data->size_) { }

bool Map_Iterator::advance(Variant *key, Variant *value) {
  if (cursor_ == limit_) {
    return false;
  } else {
    *key = data_->elms_[cursor_].key;
    *value = data_->elms_[cursor_].value;
    cursor_++;
    return true;
  }
}

bool Map_Iterator::has_next() {
  return cursor_ < data_->size();
}

pton_arena_map_t::pton_arena_map_t(Arena *origin)
  : origin_(origin)
  , size_(0)
  , capacity_(0)
  , elms_(NULL) { }

bool pton_arena_map_t::set(Variant key, Variant value) {
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

bool pton_arena_map_t::set_key_from_sink(size_t index, Variant value) {
  if (is_frozen())
    return false;
  elms_[index].key = value;
  return true;
}

bool pton_arena_map_t::set_value_from_sink(size_t index, Variant value) {
  if (is_frozen())
    return false;
  elms_[index].value = value;
  return true;
}

bool pton_arena_map_t::set(pton_sink_t **key_out, pton_sink_t **value_out) {
  size_t index = size_;
  if (!(set(Variant::null(), Variant::null())))
    return false;

  pton_sink_t *key = origin_->alloc_sink(
      tclib::new_callback(&pton_arena_map_t::set_key_from_sink, this, index));
  *key_out = key;
  pton_sink_t *value = origin_->alloc_sink(
      tclib::new_callback(&pton_arena_map_t::set_value_from_sink, this, index));
  *value_out = value;
  return true;
}

Variant pton_arena_map_t::get(Variant key) const {
  for (size_t i = 0; i < size_; i++) {
    entry_t *entry = &elms_[i];
    if (entry->key == key)
      return entry->value;
  }
  return Variant::null();
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

uint32_t Variant::string_length() const {
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

Variant Variant::string_encoding() const {
  return pton_string_encoding(value_);
}

pton_variant_t pton_string_encoding(pton_variant_t variant) {
  pton_check_binary_version(variant);
  switch (variant.header_.repr_tag_) {
    case header_t::PTON_REPR_EXTN_STRING:
      return Variant::default_string_encoding().to_c();
    case header_t::PTON_REPR_ARNA_STRING:
      return variant.payload_.as_arena_string_->encoding().to_c();
    default:
      return pton_null();
  }
}

const char *Variant::string_chars() const {
  return pton_string_chars(value_);
}

char *Variant::string_mutable_chars() const {
  return pton_string_mutable_chars(value_);
}

String::String(Variant variant) {
  if (variant.is_string())
    *static_cast<Variant*>(this) = variant;
}

pton_arena_string_t::pton_arena_string_t(char *chars, uint32_t length,
    Variant encoding, bool is_frozen)
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
  return Variant(variant).blob_size();
}

uint32_t Variant::blob_size() const {
  pton_check_binary_version(value_);
  switch (value_.header_.repr_tag_) {
    case header_t::PTON_REPR_EXTN_BLOB:
      return value_.header_.length_;
    case header_t::PTON_REPR_ARNA_BLOB:
      return value_.payload_.as_arena_blob_->size_;
    default:
      return 0;
  }
}

const void *pton_blob_data(pton_variant_t variant) {
  return Variant(variant).blob_data();
}

const void *Variant::blob_data() const {
  pton_check_binary_version(value_);
  switch (value_.header_.repr_tag_) {
    case header_t::PTON_REPR_EXTN_BLOB:
      return value_.payload_.as_external_blob_data_;
    case header_t::PTON_REPR_ARNA_BLOB:
      return value_.payload_.as_arena_blob_->data_;
    default:
      return NULL;
  }
}

void *Variant::blob_mutable_data() {
  return is_frozen() ? NULL : const_cast<void*>(blob_data());
}

Blob::Blob(Variant variant) {
  if (variant.is_blob())
    *static_cast<Variant*>(this) = variant;
}

Sink::Sink(pton_sink_t *data)
  : data_(data) { }

bool pton_sink_set(pton_sink_t *sink, pton_variant_t value) {
  pton_check_binary_version(value);
  return sink->set(value);
}

bool Sink::set(Variant value) {
  return pton_sink_set(data_, value.value_);
}

pton_variant_t pton_sink_as_array(pton_sink_t *sink) {
  return Sink(sink).as_array().to_c();
}

Array Sink::as_array() {
  if (!can_be_set())
    return Variant::null();
  Variant value = data_->origin_->new_array();
  return set(value) ? value : Variant::null();
}

pton_variant_t pton_sink_as_map(pton_sink_t *sink) {
  return Sink(sink).as_map().to_c();
}

bool Sink::can_be_set() {
  return (data_ != NULL) && data_->is_empty_;
}

Map Sink::as_map() {
  if (!can_be_set())
    return Variant::null();
  Variant value = factory()->new_map();
  return set(value) ? value : Variant::null();
}

pton_sink_t *pton_sink_new_sink(pton_sink_t *sink, pton_variant_t *out) {
  return Sink(sink).factory()->new_sink(reinterpret_cast<Variant*>(out)).to_c();
}

Factory *Sink::factory() {
  return data_->origin_;
}

bool pton_sink_set_string(pton_sink_t *sink, const char *chars, uint32_t length) {
  return Sink(sink).set_string(chars, length);
}

bool Sink::set_string(const char *chars, uint32_t length) {
  if (!can_be_set())
    return false;
  Variant value = factory()->new_string(chars, length);
  return data_->set(value);
}

pton_sink_t::pton_sink_t(Arena *origin, sink_set_callback_t on_set)
  : is_empty_(true)
  , origin_(origin)
  , on_set_(on_set) { }

bool pton_sink_t::set(Variant value) {
  if (is_empty_) {
    if (!on_set_.is_empty() && !on_set_(value))
      return false;
    is_empty_ = false;
    return true;
  } else {
    return false;
  }
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
