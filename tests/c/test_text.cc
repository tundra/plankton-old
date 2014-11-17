//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "test/asserts.hh"
#include "test/unittest.hh"
#include "plankton-inl.hh"

using namespace plankton;

#define CHECK_ASCII(EXP, VAR) do {                                             \
  TextWriter writer;                                                           \
  writer.write(VAR);                                                           \
  ASSERT_EQ(0, strcmp(EXP, *writer));                                          \
  Arena decoder;                                                               \
  TextReader parser(&decoder);                                                 \
  Variant decoded = parser.parse(*writer, writer.length());                    \
  ASSERT_TRUE(decoded.is_frozen());                                            \
  TextWriter rewriter;                                                         \
  rewriter.write(decoded);                                                     \
  ASSERT_EQ(0, strcmp(EXP, *rewriter));                                        \
} while (false)

TEST(text, primitive) {
  CHECK_ASCII("%f", Variant::no());
  CHECK_ASCII("%t", Variant::yes());
  CHECK_ASCII("%n", Variant::null());
  CHECK_ASCII("0", Variant::integer(0));
  CHECK_ASCII("10", Variant::integer(10));
  CHECK_ASCII("-10", Variant::integer(-10));
  CHECK_ASCII("fooBAR123", Variant::string("fooBAR123"));
  CHECK_ASCII("\"\"", Variant::string(""));
  CHECK_ASCII("\"123\"", Variant::string("123"));
  CHECK_ASCII("\"a b c\"", Variant::string("a b c"));
  CHECK_ASCII("\"a\\nb\"", Variant::string("a\nb"));
  CHECK_ASCII("\"a\\\"b\\\"c\"", Variant::string("a\"b\"c"));
  CHECK_ASCII("\"a\\x01b\\xa2c\"", Variant::string("a\x1" "b" "\xa2" "c"));
  CHECK_ASCII("%[TWFu]", Variant::blob("Man", 3));
  CHECK_ASCII("%[cGxlYXN1cmUu]", Variant::blob("pleasure.", 9));
  CHECK_ASCII("%[bGVhc3VyZS4=]", Variant::blob("leasure.", 8));
  CHECK_ASCII("%[ZWFzdXJlLg==]", Variant::blob("easure.", 7));
  CHECK_ASCII("%[YXN1cmUu]", Variant::blob("asure.", 6));
  CHECK_ASCII("%[c3VyZS4=]", Variant::blob("sure.", 5));
  const char *long_blob =
      "Man is distinguished, not only by his reason, but by this singular passion from "
      "other animals, which is a lust of the mind, that by a perseverance of delight "
      "in the continued and indefatigable generation of knowledge, exceeds the short "
      "vehemence of any carnal pleasure.";
  const char *long_encoded =
      "%[TWFuIGlzIGRpc3Rpbmd1aXNoZWQsIG5vdCBvbmx5IGJ5IGhpcyByZWFzb24sIGJ1dCBieSB0aGlz"
      "IHNpbmd1bGFyIHBhc3Npb24gZnJvbSBvdGhlciBhbmltYWxzLCB3aGljaCBpcyBhIGx1c3Qgb2Yg"
      "dGhlIG1pbmQsIHRoYXQgYnkgYSBwZXJzZXZlcmFuY2Ugb2YgZGVsaWdodCBpbiB0aGUgY29udGlu"
      "dWVkIGFuZCBpbmRlZmF0aWdhYmxlIGdlbmVyYXRpb24gb2Yga25vd2xlZGdlLCBleGNlZWRzIHRo"
      "ZSBzaG9ydCB2ZWhlbWVuY2Ugb2YgYW55IGNhcm5hbCBwbGVhc3VyZS4=]";
  CHECK_ASCII(long_encoded, Variant::blob(long_blob, strlen(long_blob)));
  CHECK_ASCII("%[]", Variant::blob(NULL, 0));
}

TEST(text, arrays) {
  Arena arena;
  Array a0 = arena.new_array();
  a0.add(8);
  a0.add("foo");
  CHECK_ASCII("[8, foo]", a0);
  a0.add("blahblahblah");
  CHECK_ASCII("[8, foo, blahblahblah]", a0);
  a0.add("blahblahblah");
  CHECK_ASCII("[8, foo, blahblahblah, blahblahblah]", a0);
  a0.add("blahblahblah");
  CHECK_ASCII("[8, foo, blahblahblah, blahblahblah, blahblahblah]", a0);
  a0.add("blahblahblah");
  CHECK_ASCII("[8, foo, blahblahblah, blahblahblah, blahblahblah, blahblahblah]", a0);
  a0.add("blahblahblah");
  CHECK_ASCII("[\n"
      "  8,\n"
      "  foo,\n"
      "  blahblahblah,\n"
      "  blahblahblah,\n"
      "  blahblahblah,\n"
      "  blahblahblah,\n"
      "  blahblahblah\n"
      "]", a0);
  Array a1 = arena.new_array();
  CHECK_ASCII("[]", a1);
  Array a2 = arena.new_array();
  a2.add(a1);
  a2.add(a1);
  CHECK_ASCII("[[], []]", a2);
  Array a3 = arena.new_array();
  a3.add(a2);
  a3.add(a2);
  CHECK_ASCII("[[[], []], [[], []]]", a3);
  Array a4 = arena.new_array();
  a4.add(a3);
  a4.add(a3);
  CHECK_ASCII("[[[[], []], [[], []]], [[[], []], [[], []]]]", a4);
  Array a5 = arena.new_array();
  a5.add(a4);
  a5.add(a4);
  CHECK_ASCII("[\n"
      "  [[[[], []], [[], []]], [[[], []], [[], []]]],\n"
      "  [[[[], []], [[], []]], [[[], []], [[], []]]]\n"
      "]", a5);
}

TEST(text, maps) {
  Arena arena;
  Map m0 = arena.new_map();
  m0.set("foo", "bar");
  CHECK_ASCII("{foo: bar}", m0);
  m0.set(8, 16);
  CHECK_ASCII("{foo: bar, 8: 16}", m0);
  m0.set(arena.new_array(), arena.new_map());
  CHECK_ASCII("{foo: bar, 8: 16, []: {}}", m0);
}

#define CHECK_REWRITE(IN, OUT) do {                                            \
  Arena decoder;                                                               \
  TextReader parser(&decoder);                                                 \
  Variant decoded = parser.parse(IN, strlen(IN));                              \
  TextWriter writer;                                                           \
  writer.write(decoded);                                                       \
  ASSERT_EQ(0, strcmp(OUT, *writer));                                          \
} while (false)

#define CHECK_FAILS(CHR, IN) do {                                              \
  Arena arena;                                                                 \
  TextReader parser(&arena);                                                   \
  Variant decoded = parser.parse(IN, strlen(IN));                              \
  ASSERT_TRUE(parser.has_failed());                                            \
  ASSERT_FALSE(bool(decoded));                                                 \
  ASSERT_EQ(CHR, parser.offender());                                           \
} while (false);

TEST(text, strings) {
  CHECK_REWRITE("%f", "%f");
  CHECK_REWRITE(" %f", "%f");
  CHECK_REWRITE("%f ", "%f");
  CHECK_REWRITE("[ ]", "[]");
  CHECK_REWRITE("[ 1]", "[1]");
  CHECK_REWRITE("[1 ]", "[1]");
  CHECK_REWRITE(" [1]", "[1]");
  CHECK_REWRITE("[1] ", "[1]");
  CHECK_REWRITE("[1,] ", "[1]");
  CHECK_REWRITE("{ }", "{}");
  CHECK_REWRITE("{a:b}", "{a: b}");
  CHECK_REWRITE("{ a: b}", "{a: b}");
  CHECK_REWRITE("{a: b }", "{a: b}");
  CHECK_REWRITE("{a :b}", "{a: b}");
  CHECK_REWRITE("{a: b,}", "{a: b}");
  CHECK_REWRITE("\"\\xfa\"", "\"\\xfa\"");
  CHECK_REWRITE("\"\\xFA\"", "\"\\xfa\"");
  CHECK_REWRITE("%[cGxlYXN1cmUu]", "%[cGxlYXN1cmUu]");
  CHECK_REWRITE("%[ cGxlYXN1cmUu ]", "%[cGxlYXN1cmUu]");
  CHECK_REWRITE("%[cGxl YXN1 cmUu]", "%[cGxlYXN1cmUu]");
  CHECK_REWRITE("%[ c G x l Y X N 1 c m U u ]", "%[cGxlYXN1cmUu]");
  CHECK_FAILS('%', "%f %f");
  CHECK_FAILS(',', "[,]");
  CHECK_FAILS(',', "{,}");
  CHECK_FAILS('}', "{a:}");
  CHECK_FAILS(':', "{:b}");
  CHECK_FAILS('c', "{a:b c:d}");
  CHECK_FAILS('2', "[1 2]");
  CHECK_FAILS('\0', "[1, ");
  CHECK_FAILS('\0', "[1");
  CHECK_FAILS('\0', "[");
  CHECK_FAILS('\0', "{");
  CHECK_FAILS('\0', "{a");
  CHECK_FAILS('\0', "{a:");
  CHECK_FAILS('\0', "{a:b");
  CHECK_FAILS('\0', "\"");
  CHECK_FAILS('\0', "\"\\");
  CHECK_FAILS('\0', "\"\\x");
  CHECK_FAILS('\0', "\"\\xa");
  CHECK_FAILS('g', "\"\\xag\"");
  CHECK_FAILS('g', "\"\\xga\"");
  CHECK_FAILS('%', "\"\\%\"");
  CHECK_FAILS('\0', "%");
  CHECK_FAILS('g', "%g");
  CHECK_FAILS('.', "%[cGxl.XN1cmUu]");
  CHECK_FAILS(']', "%[cGxlYXN1cmU]");
  CHECK_FAILS(']', "%[cGxlYXN1cm]");
  CHECK_FAILS(']', "%[cGxlYXN1c]");
  CHECK_FAILS('=', "%[cGxlYXN1=mUu]");
  CHECK_FAILS('=', "%[cGxlYXN1c=Uu]");
}

TEST(text, comments) {
  CHECK_REWRITE("# here comes false\n %f", "%f");
  CHECK_REWRITE("# here comes false then true %f\n %t", "%t");
  CHECK_REWRITE("# here comes false\f %f", "%f");
  CHECK_REWRITE("%f # here came false", "%f");
}
