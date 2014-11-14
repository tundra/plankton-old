//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "test/asserts.hh"
#include "test/unittest.hh"
#include "plankton-inl.hh"

using namespace plankton;

TEST(arena_cpp, alloc_values) {
  arena_t arena;
  int32_t *blocks[100];
  for (size_t i = 1; i < 100; i++) {
    int32_t *memory = arena.alloc_values<int32_t>(i);
    blocks[i] = memory;
    for (size_t j = 0; j < i; j++)
      memory[j] = i;
  }
  for (size_t i = 0; i < 100; i++) {
    int32_t *memory = blocks[i];
    for (size_t j = 0; j < i; j++)
      ASSERT_EQ(i, memory[j]);
  }
}

TEST(arena_cpp, array) {
  arena_t arena;
  array_t array = arena.new_array();
  ASSERT_FALSE(array.is_frozen());
  for (size_t i = 0; i < 100; i++) {
    ASSERT_EQ(i, array.length());
    ASSERT_TRUE(array.add(i));
    ASSERT_EQ(i + 1, array.length());
  }
  array.ensure_frozen();
  ASSERT_TRUE(array.is_frozen());
  ASSERT_FALSE(array.add(100));
  ASSERT_EQ(100, array.length());
  for (size_t i = 0; i < 100; i++) {
    variant_t elm = array[i];
    ASSERT_EQ(i, elm.integer_value());
  }
  variant_t var = array;
  ASSERT_TRUE(array == var);
  array_t array_again = array_t(var);
  ASSERT_TRUE(bool(array_again));
  ASSERT_EQ(100, array_again.length());
  array_t null_array = array_t(variant_t::null());
  ASSERT_FALSE(bool(null_array));
  ASSERT_TRUE(null_array[0] == variant_t::null());
  ASSERT_EQ(0, null_array.length());
}

TEST(arena_cpp, map) {
  arena_t arena;
  map_t map = arena.new_map();
  ASSERT_FALSE(map.is_frozen());
  for (size_t i = 0; i < 100; i++) {
    ASSERT_EQ(i, map.size());
    map.set(i, i + 3);
    ASSERT_EQ(i + 1, map.size());
  }
  map.ensure_frozen();
  ASSERT_TRUE(map.is_frozen());
  ASSERT_FALSE(map.set(1000, 1001));
  ASSERT_EQ(100, map.size());
  for (size_t i = 0; i < 100; i++) {
    variant_t elm = map[i];
    ASSERT_EQ(i + 3, elm.integer_value());
  }
  variant_t var = map;
  ASSERT_TRUE(map == var);
  map_t map_again = map_t(var);
  ASSERT_TRUE(bool(map_again));
  ASSERT_EQ(100, map_again.size());
  map_t null_map = map_t(variant_t::null());
  ASSERT_FALSE(bool(null_map));
  ASSERT_TRUE(null_map[10] == variant_t::null());
  ASSERT_EQ(0, null_map.size());
}

TEST(arena_cpp, mutstring) {
  arena_t arena;
  plankton::string_t varu8 = arena.new_string(3);
  ASSERT_FALSE(varu8.is_frozen());
  ASSERT_TRUE(varu8.encoding() == variant_t::default_string_encoding());
  plankton::string_t varai = arena.new_string(3, "ascii");
  ASSERT_FALSE(varai.is_frozen());
  ASSERT_TRUE(varai.encoding() == variant_t("ascii"));
}

TEST(arena_cpp, sink) {
  arena_t arena;
  variant_t out;
  sink_t s0 = arena.new_sink(&out);
  ASSERT_FALSE(bool(out));
  ASSERT_TRUE(s0.set(10));
  ASSERT_TRUE(bool(out));
  ASSERT_TRUE(out == variant_t::integer(10));
  ASSERT_FALSE(s0.set(12));
  ASSERT_TRUE(out == variant_t::integer(10));
}
