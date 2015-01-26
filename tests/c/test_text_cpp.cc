//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "marshal-inl.hh"
#include "plankton-inl.hh"
#include "test/asserts.hh"
#include "test/unittest.hh"
#include "utils/string-inl.h"

using namespace plankton;

static void check_syntax(TextSyntax syntax, const char *exp_src, Variant var) {
  TextWriter writer(syntax);
  writer.write(var);
  ASSERT_EQ(0, strcmp(exp_src, *writer));
  TextReader parser(syntax);
  Variant decoded = parser.parse(*writer, writer.length());
  ASSERT_TRUE(decoded.is_frozen());
  TextWriter rewriter(syntax);
  rewriter.write(decoded);
  ASSERT_EQ(0, strcmp(exp_src, *rewriter));
}

static void check_ascii(const char *exp_src, const char *exp_cmd, Variant var) {
  check_syntax(SOURCE_SYNTAX, exp_src, var);
  check_syntax(COMMAND_SYNTAX, exp_cmd == NULL ? exp_src : exp_cmd, var);
}

TEST(text_cpp, primitive) {
  check_ascii("%f", NULL, Variant::no());
  check_ascii("%t", NULL, Variant::yes());
  check_ascii("%n", NULL, Variant::null());
  check_ascii("0", NULL, Variant::integer(0));
  check_ascii("10", NULL, Variant::integer(10));
  check_ascii("-10", NULL, Variant::integer(-10));
  check_ascii("fooBAR123", NULL, Variant::string("fooBAR123"));
  check_ascii("foo-BAR-123", NULL, Variant::string("foo-BAR-123"));
  check_ascii("\"\"", NULL, Variant::string(""));
  check_ascii("\"123\"", NULL, Variant::string("123"));
  check_ascii("\"a b c\"", NULL, Variant::string("a b c"));
  check_ascii("\"a\\nb\"", NULL, Variant::string("a\nb"));
  check_ascii("\"a\\\"b\\\"c\"", NULL, Variant::string("a\"b\"c"));
  check_ascii("\"a\\x01b\\xa2c\"", NULL, Variant::string("a\x1" "b" "\xa2" "c"));
  check_ascii("%[TWFu]", NULL, Variant::blob("Man", 3));
  check_ascii("%[cGxlYXN1cmUu]", NULL, Variant::blob("pleasure.", 9));
  check_ascii("%[bGVhc3VyZS4=]", NULL, Variant::blob("leasure.", 8));
  check_ascii("%[ZWFzdXJlLg==]", NULL, Variant::blob("easure.", 7));
  check_ascii("%[YXN1cmUu]", NULL, Variant::blob("asure.", 6));
  check_ascii("%[c3VyZS4=]", NULL, Variant::blob("sure.", 5));
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
  check_ascii(long_encoded, NULL, Variant::blob(long_blob, strlen(long_blob)));
  check_ascii("%[]", NULL, Variant::blob(NULL, 0));
}

TEST(text_cpp, arrays) {
  Arena arena;
  Array a0 = arena.new_array();
  a0.add(8);
  a0.add("foo");
  check_ascii("[8, foo]", "[8 foo]", a0);
  a0.add("blahblahblah");
  check_ascii("[8, foo, blahblahblah]", "[8 foo blahblahblah]", a0);
  a0.add("blahblahblah");
  check_ascii("[8, foo, blahblahblah, blahblahblah]",
      "[8 foo blahblahblah blahblahblah]", a0);
  a0.add("blahblahblah");
  check_ascii("[8, foo, blahblahblah, blahblahblah, blahblahblah]",
      "[8 foo blahblahblah blahblahblah blahblahblah]", a0);
  a0.add("blahblahblah");
  check_ascii("[8, foo, blahblahblah, blahblahblah, blahblahblah, blahblahblah]",
      "[8 foo blahblahblah blahblahblah blahblahblah blahblahblah]", a0);
  a0.add("blahblahblah");
  check_ascii("[\n"
      "  8,\n"
      "  foo,\n"
      "  blahblahblah,\n"
      "  blahblahblah,\n"
      "  blahblahblah,\n"
      "  blahblahblah,\n"
      "  blahblahblah\n"
      "]",
      "[8 foo blahblahblah blahblahblah blahblahblah blahblahblah blahblahblah]",
      a0);
  Array a1 = arena.new_array();
  check_ascii("[]", NULL, a1);
  Array a2 = arena.new_array();
  a2.add(a1);
  a2.add(a1);
  check_ascii("[[], []]", "[[] []]", a2);
  Array a3 = arena.new_array();
  a3.add(a2);
  a3.add(a2);
  check_ascii("[[[], []], [[], []]]", "[[[] []] [[] []]]", a3);
  Array a4 = arena.new_array();
  a4.add(a3);
  a4.add(a3);
  check_ascii("[[[[], []], [[], []]], [[[], []], [[], []]]]",
      "[[[[] []] [[] []]] [[[] []] [[] []]]]",
      a4);
  Array a5 = arena.new_array();
  a5.add(a4);
  a5.add(a4);
  check_ascii("[\n"
      "  [[[[], []], [[], []]], [[[], []], [[], []]]],\n"
      "  [[[[], []], [[], []]], [[[], []], [[], []]]]\n"
      "]",
      "[[[[[] []] [[] []]] [[[] []] [[] []]]] [[[[] []] [[] []]] [[[] []] [[] []]]]]",
      a5);
}

TEST(text_cpp, maps) {
  Arena arena;
  Map m0 = arena.new_map();
  m0.set("foo", "bar");
  check_ascii("{foo: bar}", "{--foo bar}", m0);
  m0.set(8, 16);
  check_ascii("{foo: bar, 8: 16}", "{--foo bar --8 16}", m0);
  m0.set(arena.new_array(), arena.new_map());
  check_ascii("{foo: bar, 8: 16, []: {}}", "{--foo bar --8 16 --[] {}}", m0);
}

TEST(text_cpp, seeds) {
  Arena arena;
  Seed s0 = arena.new_seed();
  s0.set_header("File");
  check_ascii("@File()", "@File()", s0);
  s0.set_field("foo", "bar");
  check_ascii("@File(foo: bar)", "@File(--foo bar)", s0);
  s0.set_field(3, Variant::yes());
  check_ascii("@File(foo: bar, 3: %t)", "@File(--foo bar --3 %t)", s0);
  s0.set_field("long", "asdfkjasaslasdfsaddkjfhkasldjfhlaskdjfhlaskdjfhaasdfl");
  check_ascii("@File{\n"
      "  foo: bar,\n"
      "  3: %t,\n"
      "  long: asdfkjasaslasdfsaddkjfhkasldjfhlaskdjfhlaskdjfhaasdfl\n"
      "}",
      "@File(--foo bar --3 %t --long asdfkjasaslasdfsaddkjfhkasldjfhlaskdjfhlaskdjfhaasdfl)", s0);
}

static void check_syntax_rewrite(TextSyntax syntax, const char *src,
    const char *expected) {
  TextReader parser(syntax);
  Variant decoded = parser.parse(src, strlen(src));
  TextWriter writer(syntax);
  writer.write(decoded);
  ASSERT_EQ(0, strcmp(expected, *writer));
}

static void check_source_rewrite(const char *src, const char *expected) {
  check_syntax_rewrite(SOURCE_SYNTAX, src, expected);
}

static void check_command_rewrite(const char *src, const char *expected) {
  check_syntax_rewrite(COMMAND_SYNTAX, src, expected);
}

static void check_both_rewrite(const char *src, const char *expected) {
  check_source_rewrite(src, expected);
  check_command_rewrite(src, expected);
}

static void check_syntax_fails(TextSyntax syntax, char offender,
    int offset, const char *src) {
  TextReader parser(syntax);
  Variant decoded = parser.parse(src, strlen(src));
  ASSERT_TRUE(parser.has_failed());
  SyntaxError *error = native_cast<SyntaxError>(decoded);
  ASSERT_FALSE(error == NULL);
  ASSERT_EQ(offender, error->offender());
  ASSERT_EQ(offset, error->offset());
}

static void check_source_fails(char offender, int offset, const char *src) {
  check_syntax_fails(SOURCE_SYNTAX, offender, offset, src);
}

static void check_command_fails(char offender, int offset, const char *src) {
  check_syntax_fails(COMMAND_SYNTAX, offender, offset, src);
}

static void check_both_fail(char offender, int offset, const char *src) {
  check_source_fails(offender, offset, src);
  check_command_fails(offender, offset, src);
}

TEST(text_cpp, strings) {
  check_both_rewrite("%f", "%f");
  check_both_rewrite(" %f", "%f");
  check_both_rewrite("[ ]", "[]");
  check_both_rewrite("[ 1]", "[1]");
  check_both_rewrite("[1 ]", "[1]");
  check_both_rewrite(" [1]", "[1]");
  check_both_rewrite("[1] ", "[1]");
  check_both_rewrite("{ }", "{}");
  check_both_rewrite("\"\\xfa\"", "\"\\xfa\"");
  check_both_rewrite("\"\\xFA\"", "\"\\xfa\"");
  check_both_rewrite("%[cGxlYXN1cmUu]", "%[cGxlYXN1cmUu]");
  check_both_rewrite("%[ cGxlYXN1cmUu ]", "%[cGxlYXN1cmUu]");
  check_both_rewrite("%[cGxl YXN1 cmUu]", "%[cGxlYXN1cmUu]");
  check_both_rewrite("%[ c G x l Y X N 1 c m U u ]", "%[cGxlYXN1cmUu]");
  check_source_rewrite("[1,] ", "[1]");
  check_source_rewrite("{a:b}", "{a: b}");
  check_source_rewrite("{ a: b}", "{a: b}");
  check_source_rewrite("{a: b }", "{a: b}");
  check_source_rewrite("{a :b}", "{a: b}");
  check_source_rewrite("{a: b,}", "{a: b}");
  check_command_rewrite("{ --a b}", "{--a b}");
  check_command_rewrite("{--a b }", "{--a b}");
  check_command_rewrite("{ -- a b}", "{--a b}");
  check_both_fail('%', 3, "%f %f");
  check_source_fails(',', 1, "[,]");
  check_source_fails(',', 1, "{,}");
  check_source_fails('}', 3, "{a:}");
  check_source_fails(':', 1, "{:b}");
  check_source_fails('c', 5, "{a:b c:d}");
  check_source_fails('2', 3, "[1 2]");
  check_source_fails('\0', 4, "[1, ");
  check_source_fails('\0', 2, "[1");
  check_source_fails('\0', 1, "[");
  check_source_fails('\0', 1, "{");
  check_source_fails('\0', 2, "{a");
  check_source_fails('\0', 3, "{a:");
  check_source_fails('\0', 4, "{a:b");
  check_both_fail('\0', 1, "\"");
  check_both_fail('\0', 2, "\"\\");
  check_both_fail('\0', 3, "\"\\x");
  check_both_fail('\0', 4, "\"\\xa");
  check_both_fail('g', 4, "\"\\xag\"");
  check_both_fail('g', 3, "\"\\xga\"");
  check_both_fail('%', 2, "\"\\%\"");
  check_both_fail('\0', 1, "%");
  check_both_fail('g', 1, "%g");
  check_both_fail('.', 6, "%[cGxl.XN1cmUu]");
  check_both_fail(']', 13, "%[cGxlYXN1cmU]");
  check_both_fail(']', 12, "%[cGxlYXN1cm]");
  check_both_fail(']', 11, "%[cGxlYXN1c]");
  check_both_fail('=', 10, "%[cGxlYXN1=mUu]");
  check_both_fail('=', 11, "%[cGxlYXN1c=Uu]");
  check_command_fails('}', 4, "{--a}");
  check_command_fails('-', 5, "{--a --}");
  check_command_fails('-', 0, "--");
  check_command_fails('b', 1, "{b}");
  check_command_fails('\0', 3, "[1 ");
  check_command_fails('\0', 2, "[1");
  check_command_fails('\0', 1, "[");
  check_command_fails('\0', 1, "{");
  check_command_fails('\0', 3, "{--");
  check_command_fails('\0', 4, "{--b");
  check_command_fails('\0', 6, "{--b c");
}

TEST(text_cpp, comments) {
  check_both_rewrite("# here comes false\n %f", "%f");
  check_both_rewrite("# here comes false then true %f\n %t", "%t");
  check_both_rewrite("# here comes false\f %f", "%f");
  check_both_rewrite("%f # here came false", "%f");
  check_both_rewrite("#{ asdfas #} %f", "%f");
  check_both_rewrite("#{ \n a \n b \n c \n #} %f", "%f");
  check_both_rewrite("#{\n"
      "  # nested eol comment\n"
      "#}\n"
      "%f", "%f");
  check_both_rewrite("#{\n"
      "  # nested eol comment with ignored end marker #}\n"
      "#}\n"
      "%f", "%f");
  check_both_rewrite("#{ #{ #{ #{ deeply nested #} #} #} #} %f", "%f");
  check_both_rewrite("[ #{ asdfas #} 1 #{ asdfasd #} ]", "[1]");
  check_both_fail('\0', 5, "#{  #");
  check_both_fail('\0', 2, "#{");
  check_both_fail('\0', 1, "#");
}

#define o ,

void check_cmdline(const char *str, size_t argc, Variant *argv, size_t optc,
    Variant *optv) {
  CommandLineReader reader;
  CommandLine *cmdline = reader.parse(str, strlen(str));
  ASSERT_FALSE(cmdline == NULL);
  ASSERT_EQ(argc, cmdline->argument_count());
  for (size_t i = 0; i < argc; i++)
    ASSERT_TRUE(argv[i] == cmdline->argument(i));
  ASSERT_EQ(optc, cmdline->option_count());
  for (size_t i = 0; i < optc; i++) {
    Variant key = optv[2 * i];
    Variant value = optv[2 * i + 1];
    ASSERT_TRUE(value == cmdline->option(key));
  }
}

#define CHECK_CMDLINE(STR, ARGC, ARGV, OPTC, OPTV) do {                        \
  Variant argv[(ARGC)] = {ARGV};                                               \
  Variant optv[2 * (OPTC)] = {OPTV};                                           \
  check_cmdline((STR), (ARGC), argv, (OPTC), optv);                            \
} while (false)

TEST(text_cpp, flat_command_line) {
  CHECK_CMDLINE("", 0, , 0, );
  CHECK_CMDLINE("foo", 1, "foo", 0, );
  CHECK_CMDLINE("foo bar", 2, "foo" o "bar", 0, );
  CHECK_CMDLINE("foo bar baz", 3, "foo" o "bar" o "baz", 0, );
  CHECK_CMDLINE("foo --bar baz", 1, "foo", 1, "bar" o "baz");
  CHECK_CMDLINE("foo --bar baz --1 2", 1, "foo", 2,
      "bar" o "baz" o
      1 o 2);
}

#define CHECK_JOIN(STR, ARGC, ARGV) do {                                       \
  const char *argv[(ARGC)] = {ARGV};                                           \
  int length = 0;                                                              \
  char *joined = CommandLineReader::join_argv((ARGC), argv, &length);          \
  ASSERT_C_STREQ((STR), joined);                                                 \
  ASSERT_EQ(length, strlen(STR));                                              \
  delete[] joined;                                                             \
} while (false)

TEST(text_cpp, join) {
  CHECK_JOIN("", 0, );
  CHECK_JOIN("a", 1, "a");
  CHECK_JOIN("a b", 2, "a" o "b");
  CHECK_JOIN("a b c", 3, "a" o "b" o "c");
  CHECK_JOIN("a  c", 3, "a" o "" o "c");
  CHECK_JOIN("a     c", 3, "a" o "   " o "c");
}
