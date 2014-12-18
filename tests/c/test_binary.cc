//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "test/asserts.hh"
#include "test/unittest.hh"
#include "plankton-binary.hh"

using namespace plankton;

#define DEBUG_PRINT 0

#define CHECK_BINARY(VAR) do {                                                 \
  Variant input = (VAR);                                                       \
  BinaryWriter writer;                                                         \
  writer.write(input);                                                         \
  Arena arena;                                                                 \
  BinaryReader reader(&arena);                                                 \
  Variant decoded = reader.parse(*writer, writer.size());                      \
  ASSERT_TRUE(decoded.is_frozen());                                            \
  TextWriter input_writer;                                                     \
  input_writer.write(input);                                                   \
  TextWriter decoded_writer;                                                   \
  decoded_writer.write(decoded);                                               \
  if (DEBUG_PRINT)                                                             \
    fprintf(stderr, "%s -> %s\n", *input_writer, *decoded_writer);             \
  ASSERT_EQ(0, strcmp(*input_writer, *decoded_writer));                        \
} while (false)

#define CHECK_ENCODED(EXP, N, ...) do {                                        \
  Arena arena;                                                                 \
  BinaryReader reader(&arena);                                                 \
  uint8_t data[N] = {__VA_ARGS__};                                             \
  variant_t found = reader.parse(data, (N));                                   \
  ASSERT_TRUE(variant_t(EXP) == found);                                        \
} while (false)

TEST(binary, integers) {
  for (int i = -655; i < 655; i += 1)
    CHECK_BINARY(Variant::integer(i));
  for (int i = -6553; i < 6553; i += 12)
    CHECK_BINARY(Variant::integer(i));
  for (int i = -65536; i < 65536; i += 112)
    CHECK_BINARY(Variant::integer(i));
  for (int i = -6553600; i < 6553600; i += 11112)
    CHECK_BINARY(Variant::integer(i));
}

TEST(binary, map) {
  Arena arena;
  Map map = arena.new_map();
  CHECK_BINARY(map);
  ASSERT_TRUE(map.set(4, 5));
  CHECK_BINARY(map);
  ASSERT_TRUE(map.set(Variant::yes(), Variant::no()));
  CHECK_BINARY(map);
  Map inner = arena.new_map();
  ASSERT_TRUE(map.set(8, inner));
  CHECK_BINARY(map);
}

TEST(binary, ids) {
  CHECK_BINARY(Variant::id64(0xFABACAEA));
  CHECK_BINARY(Variant::id32(0xFABACAEA));
  CHECK_BINARY(Variant::id64(0));
  CHECK_BINARY(Variant::id64(-1));
}

TEST(binary, string_encodings) {
  Arena arena;
  Variant str = arena.new_string("foo", 3, PTON_CHARSET_SHIFT_JIS);
  BinaryWriter writer;
  writer.write(str);
  BinaryReader reader(&arena);
  Variant decoded = reader.parse(*writer, writer.size());
  ASSERT_EQ(PTON_CHARSET_SHIFT_JIS, decoded.string_encoding());
}
