//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// Inline definitions for the plankton api.

#ifndef _PLANKTON_INL
#define _PLANKTON_INL

#include "plankton.hh"

namespace plankton {

variant_t::variant_t(int64_t integer) {
  repr_tag_ = rtInteger;
  data_.as_integer_ = integer;
}

variant_t variant_t::integer(int64_t value) {
  return variant_t(value);
}

variant_t::variant_t(const char *string) {
  repr_tag_ = rtExternalString;
  data_.as_external_string_.length_ = strlen(string);
  data_.as_external_string_.chars_ = string;
}

variant_t variant_t::string(const char *string) {
  return variant_t::string(string, strlen(string));
}

variant_t::variant_t(const char *string, size_t length) {
  repr_tag_ = rtExternalString;
  data_.as_external_string_.length_ = length;
  data_.as_external_string_.chars_ = string;
}

variant_t variant_t::string(const char *string, size_t length) {
  return variant_t(string, length);
}

variant_t::variant_t(arena_array_t *arena_array) {
  repr_tag_ = rtArenaArray;
  data_.as_arena_array_ = arena_array;
}

variant_t::variant_t(arena_map_t *arena_map) {
  repr_tag_ = rtArenaMap;
  data_.as_arena_map_ = arena_map;
}

variant_t::variant_t(arena_string_t *arena_string) {
  repr_tag_ = rtArenaString;
  data_.as_arena_string_ = arena_string;
}

variant_t::variant_t(arena_blob_t *arena_blob) {
  repr_tag_ = rtArenaBlob;
  data_.as_arena_blob_ = arena_blob;
}

int64_t variant_t::integer_value() const {
  return is_integer() ? data_.as_integer_ : 0;
}

bool variant_t::bool_value() const {
  return repr_tag_ == rtTrue;
}

variant_t::operator bool() const {
  return repr_tag_ != rtNull;
}

arena_t::arena_t()
  : capacity_(0)
  , used_(0)
  , blocks_(NULL) { }

template <typename T>
T *arena_t::alloc_values(size_t elms) {
  return static_cast<T*>(alloc_raw(sizeof(T) * elms));
}

template <typename T>
T *arena_t::alloc_value() {
  return static_cast<T*>(alloc_raw(sizeof(T)));
}

} // namespace plankton

#endif // _PLANKTON
