//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// Inline definitions for the C++ plankton api.

#ifndef _PLANKTON_INL
#define _PLANKTON_INL

#include "plankton.hh"

namespace plankton {

variant_t::variant_t(int64_t integer) : value_(pton_integer(integer)) { }

variant_t variant_t::integer(int64_t value) {
  return variant_t(value);
}

variant_t::variant_t(const char *string) : value_(pton_c_str(string)) { }

variant_t variant_t::string(const char *string) {
  return pton_c_str(string);
}

variant_t::variant_t(const char *string, uint32_t length)
  : value_(pton_string(string, length)) { }

variant_t variant_t::string(const char *string, uint32_t length) {
  return variant_t(string, length);
}

int64_t variant_t::integer_value() const {
  return pton_int64_value(value_);
}

bool variant_t::bool_value() const {
  return pton_bool_value(value_);
}

variant_t::operator bool() const {
  return repr_tag() != header_t::PTON_REPR_NULL;
}

// Is this value an integer?
bool variant_t::is_integer() const {
  return pton_is_integer(value_);
}

// Is this value a map?
bool variant_t::is_map() const {
  return pton_is_map(value_);
}

// Is this value an array?
bool variant_t::is_array() const {
  return pton_is_array(value_);
}

bool variant_t::is_string() const {
  return type() == PTON_STRING;
}

bool variant_t::is_blob() const {
  return type() == PTON_BLOB;
}

} // namespace plankton

pton_arena_t::pton_arena_t()
  : capacity_(0)
  , used_(0)
  , blocks_(NULL) { }

template <typename T>
T *pton_arena_t::alloc_values(uint32_t elms) {
  return static_cast<T*>(alloc_raw(sizeof(T) * elms));
}

template <typename T>
T *pton_arena_t::alloc_value() {
  return static_cast<T*>(alloc_raw(sizeof(T)));
}

#endif // _PLANKTON
