//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "test/asserts.hh"
#include "test/unittest.hh"
#include "plankton-inl.hh"

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
