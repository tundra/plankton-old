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
  ASSERT_TRUE(socket.set_default_string_encoding(PTON_CHARSET_UTF_8));
  socket.init();
  byte_t data[16] = {112, 116, 246, 110, 0, 0, 0, 0, 1, 106, 0, 0, 0, 0, 0, 0};
  std::vector<byte_t> expected(data, data + 16);
  ASSERT_TRUE(expected == out.data());
}

TEST(socket, values) {
  ByteOutStream out;
  OutputSocket outsock(&out);
  ASSERT_TRUE(outsock.set_default_string_encoding(PTON_CHARSET_UTF_8));
  outsock.init();
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

static void handle_push_message(int *call_count, ParsedMessage *message) {
  Variant value = message->value();
  switch (*call_count) {
    case 0:
      ASSERT_TRUE(value.is_integer());
      ASSERT_EQ(10, value.integer_value());
      break;
    case 1:
      ASSERT_TRUE(value.is_string());
      ASSERT_EQ(0, strcmp("foo", value.string_chars()));
      break;
    case 2:
      ASSERT_TRUE(value.is_null());
      break;
  }
  (*call_count)++;
}

static InputStream *new_push_stream(int *call_count, InputStreamConfig *config) {
  return new PushInputStream(config, tclib::new_callback(handle_push_message, call_count));
}

TEST(socket, push_stream) {
  ByteOutStream out;
  OutputSocket outsock(&out);
  outsock.init();
  outsock.send_value(10);
  outsock.send_value("foo");
  outsock.send_value(Variant::null());
  ByteInStream in(out.data().data(), out.data().size());
  InputSocket insock(&in);
  int call_count = 0;
  insock.set_stream_factory(tclib::new_callback(new_push_stream, &call_count));
  ASSERT_TRUE(insock.init());
  while (insock.process_next_instruction())
    ;
  ASSERT_EQ(3, call_count);
}
