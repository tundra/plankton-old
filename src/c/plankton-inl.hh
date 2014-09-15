//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// Inline definitions for the C++ plankton api.

#ifndef _PLANKTON_INL
#define _PLANKTON_INL

#include "plankton.hh"
#include "plankton-inl.h"

namespace plankton {

variant_t::variant_t(int64_t integer) {
  value_ = pton_new_integer(integer);
}

variant_t variant_t::integer(int64_t value) {
  return variant_t(value);
}

variant_t::variant_t(const char *string) {
  value_.repr_tag_ = variant_p::rtExternalString;
  payload()->as_external_string_.length_ = strlen(string);
  payload()->as_external_string_.chars_ = string;
}

variant_t variant_t::string(const char *string) {
  return variant_t::string(string, strlen(string));
}

variant_t::variant_t(const char *string, size_t length) {
  value_.repr_tag_ = variant_p::rtExternalString;
  payload()->as_external_string_.length_ = length;
  payload()->as_external_string_.chars_ = string;
}

variant_t variant_t::string(const char *string, size_t length) {
  return variant_t(string, length);
}

int64_t variant_t::integer_value() const {
  return is_integer() ? payload()->as_integer_ : 0;
}

bool variant_t::bool_value() const {
  return repr_tag() == variant_p::rtTrue;
}

variant_t::operator bool() const {
  return repr_tag() != variant_p::rtNull;
}

} // namespace plankton

pton_arena_t::pton_arena_t()
  : capacity_(0)
  , used_(0)
  , blocks_(NULL) { }

template <typename T>
T *pton_arena_t::alloc_values(size_t elms) {
  return static_cast<T*>(alloc_raw(sizeof(T) * elms));
}

template <typename T>
T *pton_arena_t::alloc_value() {
  return static_cast<T*>(alloc_raw(sizeof(T)));
}

#endif // _PLANKTON
