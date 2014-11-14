//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "test/asserts.hh"
#include "test/unittest.hh"
#include "plankton-binary.hh"

BEGIN_C_INCLUDES
#include "io/file.h"
END_C_INCLUDES

#include <vector>
#include <yaml.h>

using namespace plankton;

// An individual test case read from the yaml test spec file.
class TestCase {
public:
  TestCase() { }
  TestCase(variant_t entries);

  // Returns the number of entries in this test case, discounting the clauses
  // that contained metadata like the test_type.
  size_t size() { return cases_.size(); }

  // Returns the i'th entry, skipping metadata clauses.
  variant_t operator[](size_t i) { return entries_[cases_[i]]; }

  // Returns the test type field, null if there were none.
  variant_t test_type() { return test_type_; }

private:
  array_t entries_;
  variant_t test_type_;
  std::vector<size_t> cases_;
};

TestCase::TestCase(variant_t entries) : entries_(entries) {
  for (size_t i = 0; i < entries_.length(); i++) {
    variant_t entry = entries_[i];
    variant_t test_type = entry.map_get("test_type");
    if (test_type.is_string()) {
      test_type_ = test_type;
    } else {
      cases_.push_back(i);
    }
  }
}

// Wrapper that takes care of the housekeeping involved in parsing yaml into
// plankton.
class YamlParser {
public:
  YamlParser();
  ~YamlParser();

  // Parses the contents of the given file as yaml, building the result using
  // the given sink.
  bool parse(open_file_t *in, sink_t sink);

  static bool get_test_case(variant_t name, arena_t *arena, TestCase *test_case);

private:
  // Reads the next expression.
  bool read(sink_t sink);

  // Advances the parser to the next event.
  void advance();

  // Fail if the current event is not the given type, then advance to the next
  // event.
  void expect(yaml_event_type_t type);

  // Returns true iff we're currently at the given event type.
  bool at(yaml_event_type_t type);

  // Adaptor that allows the yaml framework to read from an open_file_t. Well
  // done of them to support this by the way.
  static int handle_framework_read(void *yaml_file_ptr, unsigned char *buffer,
      size_t size, size_t *length);

  yaml_parser_t parser_;
  yaml_event_t current_;
};

YamlParser::YamlParser() {
  yaml_parser_initialize(&parser_);
}

YamlParser::~YamlParser() {
  yaml_parser_delete(&parser_);
  yaml_event_delete(&current_);
}

int YamlParser::handle_framework_read(void *yaml_file_ptr, unsigned char *buffer,
    size_t size, size_t *length) {
  open_file_t *yaml_file = (open_file_t*) yaml_file_ptr;
  int read = open_file_read_bytes(yaml_file, buffer, size);
  if (read < 0)
    return false;
  *length = read;
  return true;
}

bool YamlParser::read(sink_t sink) {
  switch (current_.type) {
    case YAML_SEQUENCE_START_EVENT: {
      expect(YAML_SEQUENCE_START_EVENT);
      array_t array = sink.as_array();
      while (!at(YAML_SEQUENCE_END_EVENT)) {
        if (!read(array.add()))
          return false;
      }
      expect(YAML_SEQUENCE_END_EVENT);
      return true;
    }
    case YAML_MAPPING_START_EVENT: {
      expect(YAML_MAPPING_START_EVENT);
      map_t map = sink.as_map();
      while (!at(YAML_MAPPING_END_EVENT)) {
        sink_t key;
        sink_t value;
        ASSERT_TRUE(map.set(&key, &value));
        if (!read(key) || !read(value))
          return false;
      }
      expect(YAML_MAPPING_END_EVENT);
      return true;
    }
    case YAML_SCALAR_EVENT: {
      // The framework isn't very helpful in this case so we have to do the
      // work of figuring out the type of the scalar ourselves.
      const char *value = (char*) current_.data.scalar.value;
      int length = current_.data.scalar.length;
      if (!current_.data.scalar.quoted_implicit) {
        // This is the case where the scalar is unquoted.
        if (strcmp("Yes", value) == 0) {
          sink.set(variant_t::yes());
          goto done;
        }
        if (strcmp("No", value) == 0) {
          sink.set(variant_t::no());
          goto done;
        }
        if (strcmp("~", value) == 0) {
          sink.set(variant_t::null());
          goto done;
        }
        char *endp = NULL;
        int as_int = strtol(value, &endp, 10);
        if (endp == (value + length)) {
          sink.set(variant_t::integer(as_int));
          goto done;
        }
      }
      // This is where we fall through to if none of the special scalar types
      // match.
      sink.set_string(value, length);
    done:
      expect(YAML_SCALAR_EVENT);
      return true;
    }
    default:
      ERROR("Unhandled: %i", current_.type);
      return false;
  }
}

void YamlParser::advance() {
  yaml_event_delete(&current_);
  yaml_parser_parse(&parser_, &current_);
}

void YamlParser::expect(yaml_event_type_t type) {
  ASSERT_EQ(type, current_.type);
  advance();
}

bool YamlParser::at(yaml_event_type_t type) {
  return current_.type == type;
}

bool YamlParser::parse(open_file_t *in, sink_t sink) {
  yaml_parser_set_input(&parser_, handle_framework_read, in);
  yaml_parser_parse(&parser_, &current_);
  expect(YAML_STREAM_START_EVENT);
  expect(YAML_DOCUMENT_START_EVENT);
  if (!read(sink))
    return false;
  expect(YAML_DOCUMENT_END_EVENT);
  expect(YAML_STREAM_END_EVENT);
  return true;
}

bool YamlParser::get_test_case(variant_t test_type, arena_t *arena, TestCase *test_case_out) {
  // Read the test case file into a variant.
  const char *yaml_path = getenv("YAML_PATH");
  open_file_t *yaml_file = file_system_open(file_system_native(), yaml_path,
      OPEN_FILE_MODE_READ);
  YamlParser parser;
  variant_t everything;
  ASSERT_TRUE(parser.parse(yaml_file, arena->new_sink(&everything)));
  open_file_close(yaml_file);
  // Scan through the test case to find the one we're looking for.
  for (size_t i = 0; i < everything.array_length(); i++) {
    variant_t test_case = everything.array_get(i).map_get("test_case");
    TestCase candidate(test_case);
    if (candidate.test_type() == test_type) {
      *test_case_out = candidate;
      return true;
    }
  }
  return false;
}

static variant_t get_variant_type_name(variant_t value) {
  switch (value.type()) {
    case PTON_INTEGER:
      return "int";
    case PTON_STRING:
      return "str";
    case PTON_NULL:
      return "null";
    case PTON_BOOL:
      return "bool";
    default:
      return "wut?";
  }
}

TEST(generic, datatypes) {
  arena_t arena;
  TestCase test_case;
  ASSERT_TRUE(YamlParser::get_test_case("datatypes", &arena, &test_case));
  for (size_t i = 0; i < test_case.size(); i++) {
    variant_t entry = test_case[i];
    variant_t value = entry.map_get("value");
    variant_t type_name = entry.map_get("type");
    ASSERT_TRUE(type_name == get_variant_type_name(value));
  }
}
