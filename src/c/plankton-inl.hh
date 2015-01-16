//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// Inline definitions for the C++ plankton api.

#ifndef _PLANKTON_INL
#define _PLANKTON_INL

#include "plankton.hh"
#include "variant-inl.hh"

namespace plankton {

Variant::Variant(int64_t integer) : value_(pton_integer(integer)) { }

Variant Variant::integer(int64_t value) {
  return Variant(value);
}

Variant::Variant(const char *string) : value_(pton_c_str(string)) { }

Variant::Variant(const char *string, uint32_t length)
  : value_(pton_string(string, length)) { }

Variant Variant::string(const char *string, uint32_t length) {
  return Variant(string, length);
}

int64_t Variant::integer_value() const {
  return pton_int64_value(value_);
}

bool Variant::bool_value() const {
  return pton_bool_value(value_);
}

Variant::operator bool() const {
  return !is_null();
}

// Is this value an integer?
bool Variant::is_integer() const {
  return pton_is_integer(value_);
}

// Is this value a map?
bool Variant::is_map() const {
  return pton_is_map(value_);
}

// Is this value an array?
bool Variant::is_array() const {
  return pton_is_array(value_);
}

bool Variant::is_seed() const {
  return type() == PTON_SEED;
}

bool Variant::is_string() const {
  return type() == PTON_STRING;
}

bool Variant::is_blob() const {
  return type() == PTON_BLOB;
}

bool Variant::is_null() const {
  return repr_tag() == header_t::PTON_REPR_NULL;
}

bool Variant::is_native() const {
  return repr_tag() == header_t::PTON_REPR_ARNA_NATIVE;
}

Arena::Arena()
  : tclib::refcount_reference_t<ArenaData>(NULL) { }

template <typename T>
T *Arena::alloc_values(uint32_t elms) {
  return static_cast<T*>(alloc_raw(sizeof(T) * elms));
}

template <typename T>
T *Arena::alloc_value() {
  return static_cast<T*>(alloc_raw(sizeof(T)));
}

} // namespace plankton

#endif // _PLANKTON
