//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// C bindings of the plankton serialization format.

#ifndef _PLANKTON_H
#define _PLANKTON_H

#include "stdc.h"

struct pton_arena_array_t;
struct pton_arena_blob_t;
struct pton_arena_value_t;
struct pton_arena_map_t;
struct pton_arena_string_t;
struct pton_arena_sink_t;
struct pton_arena_t;

union pton_variant_payload_t {
  int64_t as_integer_;
  struct {
    uint32_t length_;
    const char *chars_;
  } as_external_string_;
  struct {
    uint32_t size_;
    const void *data_;
  } as_external_blob_;
  pton_arena_value_t *as_arena_value_;
  pton_arena_array_t *as_arena_array_;
  pton_arena_map_t *as_arena_map_;
  pton_arena_string_t *as_arena_string_;
  pton_arena_blob_t *as_arena_blob_;
};

struct variant_p {
  // Tags that identify the internal representation of variants.
  enum repr_tag_t {
    rtInteger = 0x10,
    rtExternalString = 0x20,
    rtArenaString = 0x21,
    rtExternalBlob = 0x30,
    rtArenaBlob = 0x31,
    rtNull = 0x40,
    rtTrue = 0x50,
    rtFalse = 0x51,
    rtArenaArray = 0x60,
    rtArenaMap = 0x70
  };

  // This variant's tag.
  repr_tag_t repr_tag_;
  pton_variant_payload_t payload_;
};

#endif // _PLANKTON_H
