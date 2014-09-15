//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "test/asserts.hh"
#include "test/unittest.hh"

BEGIN_C_INCLUDES
#include "plankton-inl.h"
END_C_INCLUDES

TEST(variant_c, simple) {
  pton_variant_t intger = pton_integer(10);
  ASSERT_EQ(pton_variant_t::vtInteger, pton_get_type(intger));
  ASSERT_EQ(10, pton_get_integer_value(intger));
  ASSERT_EQ(0, pton_get_string_length(intger));
  ASSERT_EQ(false, pton_get_bool_value(intger));
  ASSERT_TRUE(pton_get_string_chars(intger) == NULL);
  ASSERT_TRUE(pton_is_frozen(intger));
  pton_variant_t null = pton_null();
  ASSERT_EQ(pton_variant_t::vtNull, pton_get_type(null));
  ASSERT_EQ(0, pton_get_integer_value(null));
  ASSERT_EQ(false, pton_get_bool_value(null));
  ASSERT_TRUE(pton_is_frozen(null));
  pton_variant_t str = pton_c_str("test");
  ASSERT_EQ(pton_variant_t::vtString, pton_get_type(str));
  ASSERT_EQ(0, pton_get_integer_value(str));
  ASSERT_EQ(false, pton_get_bool_value(str));
  ASSERT_TRUE(pton_is_frozen(str));
  pton_variant_t yes = pton_true();
  ASSERT_EQ(pton_variant_t::vtBool, pton_get_type(yes));
  ASSERT_EQ(true, pton_get_bool_value(yes));
  ASSERT_TRUE(pton_is_frozen(yes));
  pton_variant_t no = pton_false();
  ASSERT_EQ(pton_variant_t::vtBool, pton_get_type(no));
  ASSERT_EQ(false, pton_get_bool_value(no));
  ASSERT_TRUE(pton_is_frozen(no));
}

TEST(variant_c, equality) {
  pton_arena_t *arena = pton_new_arena();
  pton_variant_t z0 = pton_integer(0);
  pton_variant_t z1 = pton_integer(0);
  ASSERT_TRUE(pton_variants_equal(z0, z1));
  pton_variant_t sx0 = pton_c_str("x");
  ASSERT_FALSE(pton_variants_equal(z0, sx0));
  pton_variant_t sx1 = pton_c_str("x");
  ASSERT_TRUE(pton_variants_equal(sx0, sx1));
  pton_variant_t sx2 = pton_new_c_str(arena, "x");
  ASSERT_TRUE(pton_variants_equal(sx0, sx2));
  pton_variant_t sy = pton_c_str("y");
  ASSERT_FALSE(pton_variants_equal(sx0, sy));
  pton_variant_t sxy = pton_c_str("xy");
  ASSERT_FALSE(pton_variants_equal(sxy, sx0));
  ASSERT_FALSE(pton_variants_equal(sxy, sy));
  ASSERT_TRUE(pton_variants_equal(pton_null(), pton_null()));
  ASSERT_TRUE(pton_variants_equal(pton_true(), pton_true()));
  ASSERT_TRUE(pton_variants_equal(pton_false(), pton_false()));
  ASSERT_FALSE(pton_variants_equal(pton_null(), pton_false()));
  pton_variant_t a0 = pton_new_array(arena);
  ASSERT_TRUE(pton_variants_equal(a0, a0));
  pton_variant_t a1 = pton_new_array(arena);
  ASSERT_FALSE(pton_variants_equal(a0, a1));
  pton_dispose_arena(arena);
}

TEST(variant_c, blob) {
  uint8_t data[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  pton_variant_t var = pton_blob(data, 10);
  ASSERT_TRUE(pton_get_type(var) == pton_variant_t::vtBlob);
  ASSERT_EQ(10, pton_get_blob_size(var));
  ASSERT_TRUE(pton_get_blob_data(var) == data);
}
