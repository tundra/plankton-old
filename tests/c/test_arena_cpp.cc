//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "test/asserts.hh"
#include "test/unittest.hh"
#include "plankton-inl.hh"

using namespace plankton;

TEST(arena_cpp, alloc_values) {
  Arena arena;
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
  Arena arena;
  Array array = arena.new_array();
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
    Variant elm = array[i];
    ASSERT_EQ(i, elm.integer_value());
  }
  Variant var = array;
  ASSERT_TRUE(array == var);
  Array array_again = Array(var);
  ASSERT_TRUE(bool(array_again));
  ASSERT_EQ(100, array_again.length());
  Array null_array = Array(Variant::null());
  ASSERT_FALSE(bool(null_array));
  ASSERT_TRUE(null_array[0] == Variant::null());
  ASSERT_EQ(0, null_array.length());
}

TEST(arena_cpp, map) {
  Arena arena;
  Map map = arena.new_map();
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
    Variant elm = map[i];
    ASSERT_EQ(i + 3, elm.integer_value());
  }
  Variant var = map;
  ASSERT_TRUE(map == var);
  Map map_again = Map(var);
  ASSERT_TRUE(bool(map_again));
  ASSERT_EQ(100, map_again.size());
  Map null_map = Map(Variant::null());
  ASSERT_FALSE(bool(null_map));
  ASSERT_TRUE(null_map[10] == Variant::null());
  ASSERT_EQ(0, null_map.size());
}

TEST(arena_cpp, mutstring) {
  Arena arena;
  plankton::String varu8 = arena.new_string(3);
  ASSERT_FALSE(varu8.is_frozen());
  ASSERT_TRUE(varu8.encoding() == Variant::default_string_encoding());
  plankton::String varai = arena.new_string(3, PTON_CHARSET_US_ASCII);
  ASSERT_FALSE(varai.is_frozen());
  ASSERT_TRUE(varai.encoding() == PTON_CHARSET_US_ASCII);
}

TEST(arena_cpp, sink) {
  Arena arena;
  Variant out;
  Sink s0 = arena.new_sink(&out);
  ASSERT_FALSE(bool(out));
  ASSERT_TRUE(s0.set(10));
  ASSERT_TRUE(bool(out));
  ASSERT_TRUE(out == Variant::integer(10));
  ASSERT_FALSE(s0.set(12));
  ASSERT_TRUE(out == Variant::integer(10));
}

TEST(arena_cpp, adopt_inner) {
  Arena outer;
  Array arr;
  {
    Arena inner;
    arr = inner.new_array();
    arr.add(6);
    arr.add(5);
    arr.add(4);
    outer.adopt_ownership(&inner);
  }
  ASSERT_EQ(3, arr.length());
  ASSERT_EQ(6, arr[0].integer_value());
  ASSERT_EQ(5, arr[1].integer_value());
  ASSERT_EQ(4, arr[2].integer_value());
}
