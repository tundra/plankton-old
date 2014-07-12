//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "test/asserts.hh"
#include "test/unittest.hh"
#include "plankton-inl.hh"

using namespace plankton;

TEST(arena, alloc_values) {
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

TEST(arena, array) {
  arena_t arena;
  array_t array = arena.new_array();
  for (size_t i = 0; i < 100; i++) {
    ASSERT_EQ(i, array.length());
    array.add(i);
    ASSERT_EQ(i + 1, array.length());
  }
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

TEST(arena, map) {
  arena_t arena;
  map_t map = arena.new_map();
  for (size_t i = 0; i < 100; i++) {
    ASSERT_EQ(i, map.size());
    map.set(i, i + 3);
    ASSERT_EQ(i + 1, map.size());
  }
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

TEST(arena, sink) {
  arena_t arena;
  sink_t s0 = arena.new_sink();
  ASSERT_FALSE(bool(*s0));
  ASSERT_TRUE(s0.set(10));
  ASSERT_TRUE(bool(*s0));
  ASSERT_TRUE(*s0 == variant_t::integer(10));
  ASSERT_FALSE(s0.set(12));
  ASSERT_TRUE(*s0 == variant_t::integer(10));
}
