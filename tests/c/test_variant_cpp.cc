//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "test/asserts.hh"
#include "test/unittest.hh"
#include "plankton-inl.hh"

using namespace plankton;

TEST(variant_cpp, simple) {
  Variant intger = 10;
  ASSERT_EQ(PTON_INTEGER, intger.type());
  ASSERT_EQ(10, intger.integer_value());
  ASSERT_EQ(0, intger.string_length());
  ASSERT_EQ(false, intger.bool_value());
  ASSERT_TRUE(intger.string_chars() == NULL);
  ASSERT_TRUE(intger.is_frozen());
  Variant null;
  ASSERT_EQ(PTON_NULL, null.type());
  ASSERT_EQ(0, null.integer_value());
  ASSERT_EQ(false, null.bool_value());
  ASSERT_TRUE(null.is_frozen());
  Variant str = "test";
  ASSERT_EQ(PTON_STRING, str.type());
  ASSERT_EQ(0, str.integer_value());
  ASSERT_EQ(false, str.bool_value());
  ASSERT_TRUE(str.is_frozen());
  Variant yes = Variant::yes();
  ASSERT_EQ(PTON_BOOL, yes.type());
  ASSERT_EQ(true, yes.bool_value());
  ASSERT_TRUE(yes.is_frozen());
  Variant no = Variant::no();
  ASSERT_EQ(PTON_BOOL, no.type());
  ASSERT_EQ(false, no.bool_value());
  ASSERT_TRUE(no.is_frozen());
}

TEST(variant_cpp, equality) {
  Arena arena;
  Variant z0 = Variant::integer(0);
  Variant z1 = Variant::integer(0);
  ASSERT_TRUE(z0 == z1);
  Variant sx0 = "x";
  ASSERT_FALSE(z0 == sx0);
  Variant sx1 = "x";
  ASSERT_TRUE(sx0 == sx1);
  Variant sx2 = arena.new_string("x");
  ASSERT_TRUE(sx0 == sx2);
  Variant sy = "y";
  ASSERT_FALSE(sx0 == sy);
  Variant sxy = "xy";
  ASSERT_FALSE(sxy == sx0);
  ASSERT_FALSE(sxy == sy);
  ASSERT_TRUE(Variant::null() == Variant::null());
  ASSERT_TRUE(Variant::yes() == Variant::yes());
  ASSERT_TRUE(Variant::no() == Variant::no());
  ASSERT_FALSE(Variant::null() == Variant::no());
  Array a0 = arena.new_array();
  ASSERT_TRUE(a0 == a0);
  Array a1 = arena.new_array();
  ASSERT_FALSE(a0 == a1);
  Variant id0 = Variant::id64(0xDEADBEEF);
  ASSERT_TRUE(id0 == id0);
  ASSERT_FALSE(id0 == Variant::null());
  ASSERT_FALSE(id0 == Variant::no());
  ASSERT_FALSE(id0 == z0);
  Variant id1 = Variant::id64(0xDEADBEF0);
  ASSERT_FALSE(id0 == id1);
  Variant id2 = Variant::id32(0xDEADBEEF);
  ASSERT_TRUE(id0.id64_value() == id2.id64_value());
  ASSERT_FALSE(id0 == id2);
}

TEST(variant_cpp, as_bool) {
  size_t ticks = 0;
  if (Variant::null())
    ticks++;
  ASSERT_EQ(0, ticks);
  if (Variant::yes())
    ticks++;
  ASSERT_EQ(1, ticks);
  if (Variant::no())
    ticks++;
  ASSERT_EQ(2, ticks);
}

TEST(variant_cpp, blob) {
  uint8_t data[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  Variant var = Variant::blob(data, 10);
  ASSERT_TRUE(var.type() == PTON_BLOB);
  ASSERT_EQ(10, var.blob_size());
  ASSERT_TRUE(var.blob_data() == data);
}

TEST(variant_cpp, size) {
  // TODO: fix this such that msvc also gets to use compact variants.
  IF_MSVC(return,);
  ASSERT_TRUE(sizeof(Variant) <= (2 * sizeof(int64_t)));
}

TEST(variant_cpp, id64) {
  Variant var = Variant::id64(0xFABACAEA);
  ASSERT_TRUE(var.type() == PTON_ID);
  ASSERT_TRUE(var.is_frozen());
  ASSERT_EQ(64, var.id_size());
  ASSERT_EQ(0xFABACAEA, var.id64_value());
}

TEST(variant_cpp, array_sink) {
  Arena arena;
  Array array = arena.new_array();
  Sink e0 = array.add();
  Sink e1 = array.add();
  Sink e2 = array.add();
  ASSERT_EQ(3, array.length());
  ASSERT_EQ(PTON_NULL, array[0].type());
  ASSERT_EQ(PTON_NULL, array[1].type());
  ASSERT_EQ(PTON_NULL, array[2].type());
  ASSERT_TRUE(e0.set(18));
  ASSERT_FALSE(e0.set(19));
  ASSERT_EQ(PTON_INTEGER, array[0].type());
  ASSERT_EQ(PTON_NULL, array[1].type());
  ASSERT_EQ(PTON_NULL, array[2].type());
  ASSERT_TRUE(e2.set("foo"));
  ASSERT_FALSE(e2.set("bar"));
  ASSERT_EQ(PTON_INTEGER, array[0].type());
  ASSERT_EQ(PTON_NULL, array[1].type());
  ASSERT_EQ(PTON_STRING, array[2].type());
  ASSERT_TRUE(e1.set(Variant::yes()));
  ASSERT_EQ(PTON_INTEGER, array[0].type());
  ASSERT_EQ(PTON_BOOL, array[1].type());
  ASSERT_EQ(PTON_STRING, array[2].type());
}

TEST(variant_cpp, object) {
  Arena arena;
  Object obj = arena.new_object();
  ASSERT_TRUE(obj.header().is_null());
  ASSERT_TRUE(obj.set_header("foo"));
  ASSERT_TRUE(Variant("foo") == obj.header());
  ASSERT_TRUE(obj.get_field("blah").is_null());
  ASSERT_TRUE(obj.set_field("blah", 43));
  ASSERT_EQ(43, obj.get_field("blah").integer_value());
  obj.ensure_frozen();
  ASSERT_FALSE(obj.set_header("bar"));
  ASSERT_FALSE(obj.set_field("blah", 44));
  ASSERT_FALSE(obj.set_field("blub", 45));
}

TEST(variant_cpp, variant_map) {
  VariantMap<int> ints;
  ASSERT_TRUE(ints["foo"] == NULL);
  ints.set("foo", 3);
  ASSERT_EQ(3, *ints["foo"]);
  ASSERT_TRUE(ints[Variant::yes()] == NULL);
  ints.set(Variant::yes(), 4);
  ASSERT_EQ(3, *ints["foo"]);
  ASSERT_EQ(4, *ints[Variant::yes()]);
  ints.set("foo", 5);
  ASSERT_EQ(5, *ints["foo"]);
  ASSERT_EQ(4, *ints[Variant::yes()]);
  ASSERT_TRUE(ints[Variant::null()] == NULL);
  ints.set(Variant::null(), 6);
  ASSERT_EQ(5, *ints["foo"]);
  ASSERT_EQ(4, *ints[Variant::yes()]);
  ASSERT_EQ(6, *ints[Variant::null()]);
}
