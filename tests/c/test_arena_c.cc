//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "test/asserts.hh"
#include "test/unittest.hh"

BEGIN_C_INCLUDES
#include "plankton.h"
END_C_INCLUDES

TEST(arena_c, array) {
  pton_arena_t *arena = pton_new_arena();
  pton_variant_t array = pton_new_array(arena);
  ASSERT_FALSE(pton_is_frozen(array));
  for (size_t i = 0; i < 100; i++) {
    ASSERT_EQ(i, pton_array_length(array));
    ASSERT_TRUE(pton_array_add(array, pton_integer(i)));
    ASSERT_EQ(i + 1, pton_array_length(array));
  }
  pton_ensure_frozen(array);
  ASSERT_TRUE(pton_is_frozen(array));
  ASSERT_FALSE(pton_array_add(array, pton_integer(100)));
  ASSERT_EQ(100, pton_array_length(array));
  for (size_t i = 0; i < 100; i++) {
    pton_variant_t elm = pton_array_get(array, i);
    ASSERT_EQ(i, pton_int64_value(elm));
  }
  pton_dispose_arena(arena);
}

TEST(arena_c, map) {
  pton_arena_t *arena = pton_new_arena();
  pton_variant_t map = pton_new_map(arena);
  ASSERT_FALSE(pton_is_frozen(map));
  for (size_t i = 0; i < 100; i++) {
    ASSERT_EQ(i, pton_map_size(map));
    ASSERT_TRUE(pton_map_set(map, pton_integer(i), pton_integer(i + 3)));
    ASSERT_EQ(i + 1, pton_map_size(map));
  }
  pton_ensure_frozen(map);
  ASSERT_TRUE(pton_is_frozen(map));
  ASSERT_FALSE(pton_map_set(map, pton_integer(1000), pton_integer(1001)));
  ASSERT_EQ(100, pton_map_size(map));
  for (size_t i = 0; i < 100; i++) {
    pton_variant_t elm = pton_map_get(map, pton_integer(i));
    ASSERT_EQ(i + 3, pton_int64_value(elm));
  }
  pton_dispose_arena(arena);
}

TEST(arena_c, mutstring) {
  pton_arena_t *arena = pton_new_arena();
  pton_variant_t var = pton_new_mutable_string(arena, 3);
  ASSERT_FALSE(pton_is_frozen(var));
  pton_dispose_arena(arena);
}

TEST(arena_c, sink) {
  pton_arena_t *carena = pton_new_arena();
  pton_variant_t out = pton_null();
  pton_sink_t *cs0 = pton_new_sink(carena, &out);
  ASSERT_TRUE(pton_variants_equal(out, pton_null()));
  ASSERT_TRUE(pton_sink_set(cs0, pton_integer(10)));
  ASSERT_TRUE(pton_variants_equal(out, pton_integer(10)));
  ASSERT_FALSE(pton_sink_set(cs0, pton_integer(12)));
  ASSERT_TRUE(pton_variants_equal(out, pton_integer(10)));
  pton_dispose_arena(carena);
}
