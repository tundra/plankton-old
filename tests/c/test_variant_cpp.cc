//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "test/asserts.hh"
#include "test/unittest.hh"
#include "plankton-inl.hh"

using namespace plankton;

TEST(variant_cpp, simple) {
  variant_t intger = 10;
  ASSERT_EQ(PTON_INTEGER, intger.type());
  ASSERT_EQ(10, intger.integer_value());
  ASSERT_EQ(0, intger.string_length());
  ASSERT_EQ(false, intger.bool_value());
  ASSERT_TRUE(intger.string_chars() == NULL);
  ASSERT_TRUE(intger.is_frozen());
  variant_t null;
  ASSERT_EQ(PTON_NULL, null.type());
  ASSERT_EQ(0, null.integer_value());
  ASSERT_EQ(false, null.bool_value());
  ASSERT_TRUE(null.is_frozen());
  variant_t str = "test";
  ASSERT_EQ(PTON_STRING, str.type());
  ASSERT_EQ(0, str.integer_value());
  ASSERT_EQ(false, str.bool_value());
  ASSERT_TRUE(str.is_frozen());
  variant_t yes = variant_t::yes();
  ASSERT_EQ(PTON_BOOL, yes.type());
  ASSERT_EQ(true, yes.bool_value());
  ASSERT_TRUE(yes.is_frozen());
  variant_t no = variant_t::no();
  ASSERT_EQ(PTON_BOOL, no.type());
  ASSERT_EQ(false, no.bool_value());
  ASSERT_TRUE(no.is_frozen());
}

TEST(variant_cpp, equality) {
  arena_t arena;
  variant_t z0 = variant_t::integer(0);
  variant_t z1 = variant_t::integer(0);
  ASSERT_TRUE(z0 == z1);
  variant_t sx0 = "x";
  ASSERT_FALSE(z0 == sx0);
  variant_t sx1 = "x";
  ASSERT_TRUE(sx0 == sx1);
  variant_t sx2 = arena.new_string("x");
  ASSERT_TRUE(sx0 == sx2);
  variant_t sy = "y";
  ASSERT_FALSE(sx0 == sy);
  variant_t sxy = "xy";
  ASSERT_FALSE(sxy == sx0);
  ASSERT_FALSE(sxy == sy);
  ASSERT_TRUE(variant_t::null() == variant_t::null());
  ASSERT_TRUE(variant_t::yes() == variant_t::yes());
  ASSERT_TRUE(variant_t::no() == variant_t::no());
  ASSERT_FALSE(variant_t::null() == variant_t::no());
  array_t a0 = arena.new_array();
  ASSERT_TRUE(a0 == a0);
  array_t a1 = arena.new_array();
  ASSERT_FALSE(a0 == a1);
  variant_t id0 = variant_t::id64(0xDEADBEEF);
  ASSERT_TRUE(id0 == id0);
  ASSERT_FALSE(id0 == variant_t::null());
  ASSERT_FALSE(id0 == variant_t::no());
  ASSERT_FALSE(id0 == z0);
  variant_t id1 = variant_t::id64(0xDEADBEF0);
  ASSERT_FALSE(id0 == id1);
  variant_t id2 = variant_t::id32(0xDEADBEEF);
  ASSERT_TRUE(id0.id64_value() == id2.id64_value());
  ASSERT_FALSE(id0 == id2);
}

TEST(variant_cpp, as_bool) {
  size_t ticks = 0;
  if (variant_t::null())
    ticks++;
  ASSERT_EQ(0, ticks);
  if (variant_t::yes())
    ticks++;
  ASSERT_EQ(1, ticks);
  if (variant_t::no())
    ticks++;
  ASSERT_EQ(2, ticks);
}

TEST(variant_cpp, blob) {
  uint8_t data[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  variant_t var = variant_t::blob(data, 10);
  ASSERT_TRUE(var.type() == PTON_BLOB);
  ASSERT_EQ(10, var.blob_size());
  ASSERT_TRUE(var.blob_data() == data);
}

TEST(variant_cpp, size) {
  // TODO: fix this such that msvc also gets to use compact variants.
  IF_MSVC(return,);
  ASSERT_TRUE(sizeof(variant_t) <= (2 * sizeof(int64_t)));
}

TEST(variant_cpp, id64) {
  variant_t var = variant_t::id64(0xFABACAEA);
  ASSERT_TRUE(var.type() == PTON_ID);
  ASSERT_TRUE(var.is_frozen());
  ASSERT_EQ(64, var.id_size());
  ASSERT_EQ(0xFABACAEA, var.id64_value());
}