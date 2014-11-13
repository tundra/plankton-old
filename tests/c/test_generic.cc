//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "test/asserts.hh"
#include "test/unittest.hh"
#include "plankton-binary.hh"

BEGIN_C_INCLUDES
#include "io/file.h"
END_C_INCLUDES

#include <yaml.h>

using namespace plankton;

class YamlParser {
public:
  YamlParser();
  ~YamlParser();
  variant_t parse(open_file_t *in);
private:
  yaml_parser_t parser_;
};

YamlParser::YamlParser() {
  yaml_parser_initialize(&parser_);
}

YamlParser::~YamlParser() {
  yaml_parser_delete(&parser_);
}

static int handle_framework_read(void *yaml_file_ptr, unsigned char *buffer,
    size_t size, size_t *length) {
  open_file_t *yaml_file = (open_file_t*) yaml_file_ptr;
  int read = open_file_read_bytes(yaml_file, buffer, size);
  if (read < 0)
    return false;
  *length = read;
  return true;
}

variant_t YamlParser::parse(open_file_t *in) {
  yaml_parser_set_input(&parser_, handle_framework_read, in);
  yaml_event_t event;
  yaml_parser_parse(&parser_, &event);
  return variant_t::null();
}

TEST(generic, datatypes) {
  const char *yaml_path = getenv("YAML_PATH");
  open_file_t *yaml_file = file_system_open(file_system_native(), yaml_path,
      OPEN_FILE_MODE_READ);
  YamlParser parser;
  parser.parse(yaml_file);
  open_file_close(yaml_file);
}
