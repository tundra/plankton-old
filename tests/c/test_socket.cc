//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "test/asserts.hh"
#include "test/unittest.hh"
#include "plankton-inl.hh"
#include "socket.hh"

using namespace plankton;
using namespace tclib;

TEST(socket, header) {
  ByteOutStream out;
  OutputSocket socket(&out);
  socket.init();
  socket.set_default_string_encoding(PTON_CHARSET_UTF_8);
  byte_t data[16] = {112, 116, 246, 110, 0, 0, 0, 0, 1, 106, 0, 0, 0, 0, 0, 0};
  std::vector<byte_t> expected(data, data + 16);
  ASSERT_TRUE(expected == out.data());
}

TEST(socket, values) {
  ByteOutStream out;
  OutputSocket outsock(&out);
  outsock.init();
  outsock.set_default_string_encoding(PTON_CHARSET_UTF_8);
  Arena arena;
  Array aout = arena.new_array();
  aout.add(Variant::null());
  aout.add(Variant::integer(42));
  outsock.send_value(aout, Variant::null());
  ByteInStream in(out.data().data(), out.data().size());
  InputSocket insock(&in);
  ASSERT_TRUE(insock.init());
  while (insock.process_next_instruction())
    ;
  BufferInputStream *root_stream = static_cast<BufferInputStream*>(insock.root_stream());
  ASSERT_FALSE(root_stream == NULL);
  Array ain = root_stream->pull_message(&arena);
  ASSERT_EQ(2, ain.length());
  ASSERT_TRUE(ain[0].is_null());
  ASSERT_EQ(42, ain[1].integer_value());
}
