//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "test/asserts.hh"
#include "test/unittest.hh"

BEGIN_C_INCLUDES
#include "plankton.h"
#include "utils/string-inl.h"
END_C_INCLUDES

TEST(text_c, primitive) {
  pton_command_line_reader_t *reader = pton_new_command_line_reader();
  const char *argv[6] = {"foo", "--bar", "baz", "6", "--1", "2"};
  pton_command_line_t *cmdline = pton_command_line_reader_parse(reader, 6, argv);
  ASSERT_EQ(2, pton_command_line_argument_count(cmdline));
  ASSERT_C_STREQ("foo", pton_string_chars(pton_command_line_argument(cmdline, 0)));
  ASSERT_EQ(6, pton_int64_value(pton_command_line_argument(cmdline, 1)));
  ASSERT_EQ(PTON_NULL, pton_type(pton_command_line_argument(cmdline, 2)));
  ASSERT_EQ(2, pton_command_line_option_count(cmdline));
  ASSERT_C_STREQ("baz", pton_string_chars(pton_command_line_option(
      cmdline, pton_c_str("bar"), pton_null())));
  ASSERT_EQ(2, pton_int64_value(pton_command_line_option(
      cmdline, pton_integer(1), pton_null())));
  pton_dispose_command_line_reader(reader);
}
