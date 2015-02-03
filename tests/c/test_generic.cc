//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "test/asserts.hh"
#include "test/unittest.hh"
#include "plankton-binary.hh"

BEGIN_C_INCLUDES
#include "io/file.h"
#include "utils/string-inl.h"
END_C_INCLUDES

#include <vector>
#include <yaml.h>

using namespace plankton;

// An individual test case read from the yaml test spec file.
class TestCase {
public:
  TestCase() { }
  TestCase(Variant entries);

  // Returns the number of entries in this test case, discounting the clauses
  // that contained metadata like the test_type.
  size_t size() { return cases_.size(); }

  // Returns the i'th entry, skipping metadata clauses.
  Variant operator[](size_t i) { return entries_[cases_[i]]; }

  // Returns the test type field, null if there were none.
  Variant test_type() { return test_type_; }

private:
  Array entries_;
  Variant test_type_;
  std::vector<size_t> cases_;
};

TestCase::TestCase(Variant entries) : entries_(entries) {
  for (size_t i = 0; i < entries_.length(); i++) {
    Variant entry = entries_[i];
    Variant test_type = entry.map_get("test_type");
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
  bool parse(in_stream_t *in, Sink sink);

  static bool get_test_case(Variant name, Arena *arena, TestCase *test_case);

private:
  // Reads the next expression.
  bool read(Sink sink);

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
  in_stream_t *yaml_file = (in_stream_t*) yaml_file_ptr;
  int read = in_stream_read_bytes(yaml_file, buffer, size);
  if (read < 0)
    return false;
  *length = read;
  return true;
}

static bool adapt_seed(Array fields, Sink sink) {
  Seed result = sink.as_seed();
  for (size_t i = 0; i < fields.length(); i++) {
    Map field = fields[i];
    ASSERT_EQ(1, field.size());
    Variant as_type = field["type"];
    if (!as_type.is_null()) {
      result.set_header(as_type);
    } else {
      Map::Iterator iter = field.begin();
      ASSERT_TRUE(iter != field.end());
      result.set_field(iter->key(), iter->value());
    }
  }
  return true;
}

static bool adapt_blob(Array bytes, Sink sink) {
  Blob result = sink.as_blob(bytes.length());
  uint8_t *data = static_cast<uint8_t*>(result.mutable_data());
  for (size_t i = 0; i < bytes.length(); i++) {
    Variant byte = bytes[i];
    data[i] = byte.integer_value();
  }
  return true;
}

bool YamlParser::read(Sink sink) {
  switch (current_.type) {
    case YAML_SEQUENCE_START_EVENT: {
      expect(YAML_SEQUENCE_START_EVENT);
      Array array = sink.as_array();
      while (!at(YAML_SEQUENCE_END_EVENT)) {
        if (!read(array.add()))
          return false;
      }
      expect(YAML_SEQUENCE_END_EVENT);
      return true;
    }
    case YAML_MAPPING_START_EVENT: {
      expect(YAML_MAPPING_START_EVENT);
      Variant raw_map;
      Map map = sink.factory()->new_sink(&raw_map).as_map();
      while (!at(YAML_MAPPING_END_EVENT)) {
        Sink key;
        Sink value;
        ASSERT_TRUE(map.set(&key, &value));
        if (!read(key) || !read(value))
          return false;
      }
      expect(YAML_MAPPING_END_EVENT);
      if (map.size() == 1) {
        Variant as_object = map["object"];
        if (as_object.is_array())
          return adapt_seed(as_object, sink);
        Variant as_blob = map["blob"];
        if (as_blob.is_array())
          return adapt_blob(as_blob, sink);
      }
      sink.set(raw_map);
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
          sink.set(Variant::yes());
          goto done;
        }
        if (strcmp("No", value) == 0) {
          sink.set(Variant::no());
          goto done;
        }
        if (strcmp("~", value) == 0) {
          sink.set(Variant::null());
          goto done;
        }
        char *endp = NULL;
        int64_t as_int = strtol(value, &endp, 10);
        if (endp == (value + length)) {
          sink.set(Variant::integer(as_int));
          goto done;
        }
        if (length >= 2 && value[0] == '0' && value[1] == 'x') {
          int64_t as_hex = strtol(value + 2, &endp, 16);
          if (endp == (value + length)) {
            sink.set(Variant::integer(as_hex));
            goto done;
          }
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

bool YamlParser::parse(in_stream_t *in, Sink sink) {
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

bool YamlParser::get_test_case(Variant test_type, Arena *arena, TestCase *test_case_out) {
  // Read the test case file into a variant.
  utf8_t yaml_path = new_c_string(getenv("YAML_PATH"));
  file_streams_t streams = file_system_open(file_system_native(), yaml_path,
      OPEN_FILE_MODE_READ);
  YamlParser parser;
  Variant everything;
  ASSERT_TRUE(parser.parse(streams.in, arena->new_sink(&everything)));
  file_streams_close(&streams);
  // Scan through the test case to find the one we're looking for.
  for (size_t i = 0; i < everything.array_length(); i++) {
    Variant test_case = everything.array_get(i).map_get("test_case");
    TestCase candidate(test_case);
    if (candidate.test_type() == test_type) {
      *test_case_out = candidate;
      return true;
    }
  }
  return false;
}

static Variant get_variant_type_name(Variant value) {
  switch (value.type()) {
    case PTON_INTEGER:
      return "int";
    case PTON_STRING:
      return "str";
    case PTON_NULL:
      return "null";
    case PTON_BOOL:
      return "bool";
    case PTON_ARRAY:
      return "array";
    case PTON_MAP:
      return "map";
    case PTON_SEED:
      return "obj";
    case PTON_BLOB:
      return "blob";
    default:
      return "wut?";
  }
}

void fail_assert_samevar(const char *file, int line, Variant a, Variant b,
    const char *a_src, const char *b_src) {
  TextWriter a_writer;
  a_writer.write(a);
  TextWriter b_writer;
  b_writer.write(b);
  fail(file, line, "Assertion failed: %s == %s.\n  Expected: %s\n  Found: %s",
      a_src, b_src, *a_writer, *b_writer);
}

#define ASSERT_SAMEVAR(A, B) do {                                              \
  Variant __a__ = (A);                                                         \
  Variant __b__ = (B);                                                         \
  if (!(__a__ == __b__))                                                       \
    fail_assert_samevar(__FILE__, __LINE__, __a__, __b__, #A, #B);             \
} while (false)

TEST(generic, datatypes) {
  Arena arena;
  TestCase test_case;
  ASSERT_TRUE(YamlParser::get_test_case("datatypes", &arena, &test_case));
  for (size_t i = 0; i < test_case.size(); i++) {
    Variant entry = test_case[i];
    Variant value = entry.map_get("value");
    Variant type_name = entry.map_get("type");
    ASSERT_SAMEVAR(type_name, get_variant_type_name(value));
  }
}

TEST(generic, transcoding) {
  Arena arena;
  TestCase test_case;
  ASSERT_TRUE(YamlParser::get_test_case("transcoding", &arena, &test_case));
  for (size_t i = 0; i < test_case.size(); i++) {
    Variant entry = test_case[i];
    Variant value = entry.map_get("value");
    Variant binary_expected = entry.map_get("binary");
    BinaryWriter writer;
    writer.write(value);
    ASSERT_EQ(binary_expected.array_length(), writer.size());
    for (size_t i = 0; i < writer.size(); i++)
      ASSERT_EQ(binary_expected.array_get(i).integer_value(), (*writer)[i]);
  }
}
