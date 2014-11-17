//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "plankton-inl.hh"
#include "stdc.h"
#include "utils-inl.hh"
#include <new>

namespace plankton {

TextWriter::TextWriter()
  : chars_(NULL)
  , length_(0) { }

TextWriter::~TextWriter() {
  delete[] chars_;
  chars_ = NULL;
}

// Utility used to print plankton values as ascii.
class TextWriterImpl {
public:
  TextWriterImpl();

  // Writes the given value on the stream at the current indentation level.
  void write(Variant value);

  // Null-terminates and stores the result in the destination.
  void flush(TextWriter *writer);

  // Is the given character allowed as the first character of an unquoted string?
  static bool is_unquoted_string_start(char c);

  // Is the given character allowed as a subsequent character of an unquoted
  // string?
  static bool is_unquoted_string_part(char c);

private:
  // Writes the given null-terminated string directly to the buffer.
  void write_raw_string(const char *chars);

  // Writes the given character directly to the buffer.
  void write_raw_char(char c);

  // Writes any pending newlines.
  void flush_pending_newline();

  // Schedules a newline to be printed before the next non-newline character.
  void schedule_newline();

  // Increases the indentation level.
  void indent();

  // Decreases the indentation level.
  void deindent();

  // Writes the given byte as two hex characters.
  void write_hex_byte(uint8_t c);

  // Writes the given integer in decimal.
  void write_integer(int64_t value);

  // Writes the given string, properly ascii encoded.
  void write_string(const char *chars, size_t length);

  // Is the given character one that must be escaped within a string?
  static bool is_unescaped_char(char c);

  // If the given string one that can be written without double quotes?
  static bool is_unquoted(const char *chars, size_t length);

  // Given a character, stores the short escape character to use (for instance,
  // 'n' for newline) in the out parameter and returns true, or returns false
  // if this one doesn't have a short escape.
  static bool encode_short_escape(char c, char *out);

  // Base64-encodes and writes the given blob.
  void write_blob(const void *data, size_t size);

  // Writes an array value.
  void write_array(Array array);

  // Writes a map value.
  void write_map(Map map);

  // Writes the given identity token.
  void write_id(uint32_t size, uint64_t value);

  // Ensures that the buffer has enough capacity to hold the given number of
  // additional characters.
  void ensure_capacity(size_t size);

  // The characters to use for base-64 encoding (the length is 65 to account for
  // the null terminator).
  static const char kBase64Chars[65];

  // The character to use for base-64 padding.
  static const char kBase64Padding;

  // Lengths up to (but not including) this will be considered short. Longer
  // lengths are considered, for all practical purposes, infinite.
  static const size_t kShortLengthLimit = 80;

  // Returns the length of the given variant, added to the given offset. The
  // short length limit is treated as infinity so if we ever reach a value
  // greater we bail out immediately. This is to keep the calculation constant
  // and avoid the potential complexity blowup if we compute the full size
  // of subtrees for every variant.
  static size_t get_short_length(Variant variant, size_t offset);

  Buffer<char> chars_;
  size_t indent_;
  bool has_pending_newline_;
};

TextWriterImpl::TextWriterImpl()
  : indent_(0)
  , has_pending_newline_(false) { }

void TextWriterImpl::write(Variant value) {
  switch (value.type()) {
    case PTON_BOOL:
      write_raw_string(value.bool_value() ? "%t" : "%f");
      break;
    case PTON_NULL:
      write_raw_string("%n");
      break;
    case PTON_INTEGER:
      write_integer(value.integer_value());
      break;
    case PTON_STRING:
      write_string(value.string_chars(), value.string_length());
      break;
    case PTON_ID:
      write_id(value.id_size(), value.id64_value());
      break;
    case PTON_BLOB:
      write_blob(value.blob_data(), value.blob_size());
      break;
    case PTON_ARRAY:
      write_array(Array(value));
      break;
    case PTON_MAP:
      write_map(Map(value));
      break;
    default:
      write_raw_string("?");
      break;
  }
}

size_t TextWriterImpl::get_short_length(Variant value, size_t offset) {
  switch (value.type()) {
    case PTON_INTEGER:
      return offset + 5;
    case PTON_BOOL:
    case PTON_NULL:
      return offset + 2;
    case PTON_STRING:
      return offset + value.string_length();
    case PTON_ARRAY: {
      Array array = Array(value);
      size_t current = offset + 2;
      for (size_t i = 0; i < array.length() && current < kShortLengthLimit; i++)
        current = get_short_length(array[i], current) + 2;
      return current;
    }
    case PTON_MAP: {
      Map map = Map(value);
      size_t current = offset + 2;
      Variant key, value;
      for (Map::Iterator iter = map.iter();
           iter.advance(&key, &value) && current < kShortLengthLimit;) {
        current = get_short_length(key, current) + 2;
        current = get_short_length(value, current);
      }
      return current;
    }
    default:
      return kShortLengthLimit;
  }
}

void TextWriterImpl::write_raw_string(const char *chars) {
  flush_pending_newline();
  size_t size = strlen(chars);
  chars_.write(chars, size);
}

void TextWriterImpl::write_raw_char(char c) {
  flush_pending_newline();
  chars_.add(c);
}

void TextWriterImpl::flush_pending_newline() {
  if (!has_pending_newline_)
    return;
  chars_.add('\n');
  chars_.fill(' ', indent_);
  has_pending_newline_ = false;
}

void TextWriterImpl::schedule_newline() {
  has_pending_newline_ = true;
}

void TextWriterImpl::indent() {
  indent_ += 2;
}

void TextWriterImpl::deindent() {
  indent_ -= 2;
}

void TextWriterImpl::write_hex_byte(uint8_t c) {
  uint8_t high = (c >> 4) & 0xF;
  write_raw_char(high < 10 ? ('0' + high) : ('a' + high - 10));
  uint8_t low = c & 0xF;
  write_raw_char(low < 10 ? ('0' + low) : ('a' + low - 10));
}

void TextWriterImpl::write_integer(int64_t value) {
  char chars[64];
  sprintf(chars, "%li", value);
  write_raw_string(chars);
}

bool TextWriterImpl::is_unquoted_string_start(char c) {
  return ('a' <= c && c <= 'z')
      || ('A' <= c && c <= 'Z');
}

bool TextWriterImpl::is_unquoted_string_part(char c) {
  return is_unquoted_string_start(c)
      || ('0' <= c && c <= '9')
      || (c == '_');
}

bool TextWriterImpl::is_unescaped_char(char c) {
  if (c < ' ' || c > '~') {
    return false;
  } else {
    return (c != '"' && c != '\\');
  }
}

bool TextWriterImpl::is_unquoted(const char *chars, size_t length) {
  if (length == 0)
    return false;
  if (!is_unquoted_string_start(chars[0]))
    return false;
  for (size_t i = 1; i < length; i++) {
    if (!is_unquoted_string_part(chars[i]))
      return false;
  }
  return true;
}

void TextWriterImpl::write_string(const char *chars, size_t length) {
  if (is_unquoted(chars, length)) {
    flush_pending_newline();
    chars_.write(chars, length);
    return;
  }
  // Iterate through the characters one block at a time. A block is either a
  // contiguous range of ascii characters (as determined by is_ascii_char) or
  // a single non-ascii character.
  write_raw_char('"');
  for (size_t bi = 0; bi < length;) {
    if (is_unescaped_char(chars[bi])) {
      write_raw_char(chars[bi++]);
      while (bi < length && is_unescaped_char(chars[bi]))
        write_raw_char(chars[bi++]);
    } else {
      write_raw_char('\\');
      char c = chars[bi];
      char short_escape = '\0';
      if (encode_short_escape(c, &short_escape)) {
        write_raw_char(short_escape);
      } else {
        write_raw_char('x');
        write_hex_byte(c);
      }
      bi++;
    }
  }
  write_raw_char('"');
}

const char TextWriterImpl::kBase64Chars[65] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789"
    "+/";

const char TextWriterImpl::kBase64Padding = '=';

void TextWriterImpl::write_blob(const void *data, size_t size) {
  const uint8_t *bytes = static_cast<const uint8_t*>(data);
  write_raw_char('%');
  write_raw_char('[');
  size_t i;
  for (i = 0; (i + 2) < size; i += 3) {
    // First emit all full blocks of 3 bytes.
    uint32_t word = (bytes[i] << 16) + (bytes[i+1] << 8) + bytes[i+2];
    write_raw_char(kBase64Chars[(word >> 18) & 0x3F]);
    write_raw_char(kBase64Chars[(word >> 12) & 0x3F]);
    write_raw_char(kBase64Chars[(word >>  6) & 0x3F]);
    write_raw_char(kBase64Chars[(word >>  0) & 0x3F]);
  }
  switch (size - i) {
    case 0:
      // The data was aligned so we're done.
      break;
    case 1: {
      // There's a single byte left.
      uint32_t word = (bytes[i] << 16);
      write_raw_char(kBase64Chars[(word >> 18) & 0x3F]);
      write_raw_char(kBase64Chars[(word >> 12) & 0x3F]);
      write_raw_char(kBase64Padding);
      write_raw_char(kBase64Padding);
      break;
    }
    case 2: {
      // There's two bytes left.
      uint32_t word = (bytes[i] << 16) + (bytes[i+1] << 8);
      write_raw_char(kBase64Chars[(word >> 18) & 0x3F]);
      write_raw_char(kBase64Chars[(word >> 12) & 0x3F]);
      write_raw_char(kBase64Chars[(word >>  6) & 0x3F]);
      write_raw_char(kBase64Padding);
      break;
    }
  }
  write_raw_char(']');
}

void TextWriterImpl::write_array(Array array) {
  bool is_long = get_short_length(array, indent_) >= kShortLengthLimit;
  write_raw_char('[');
  if (is_long) {
    indent();
    schedule_newline();
  }
  for (size_t i = 0; i < array.length(); i++) {
    Variant value = array[i];
    write(value);
    if (i + 1 < array.length()) {
      write_raw_char(',');
      if (!is_long)
        write_raw_char(' ');
    }
    if (is_long)
      schedule_newline();
  }
  if (is_long)
    deindent();
  write_raw_char(']');
}

void TextWriterImpl::write_map(Map map) {
  bool is_long = get_short_length(map, indent_) >= kShortLengthLimit;
  write_raw_char('{');
  if (is_long) {
    indent();
    schedule_newline();
  }
  Variant key;
  Variant value;
  for (Map::Iterator iter = map.iter(); iter.advance(&key, &value);) {
    write(key);
    write_raw_char(':');
    write_raw_char(' ');
    write(value);
    if (iter.has_next()) {
      write_raw_char(',');
      if (!is_long)
        write_raw_char(' ');
    }
    if (is_long)
      schedule_newline();
  }
  if (is_long)
    deindent();
  write_raw_char('}');
}

void TextWriterImpl::write_id(uint32_t size, uint64_t value) {
  char chars[64];
  switch (size) {
    case 64:
      sprintf(chars, "~%016lx", value);
      break;
    case 32:
      sprintf(chars, "~%08lx", value);
      break;
    case 16:
      sprintf(chars, "~%04lx", value);
      break;
    case 8:
      sprintf(chars, "~%02lx", value);
      break;
    default:
      sprintf(chars, "~%i:%lx", size, value);
      break;
  }
  write_raw_string(chars);
}

void TextWriterImpl::flush(TextWriter *writer) {
  flush_pending_newline();
  writer->length_ = chars_.length();
  chars_.add('\0');
  writer->chars_ = chars_.release();
}

void TextWriter::write(Variant value) {
  TextWriterImpl impl;
  impl.write(value);
  impl.flush(this);
}

// Utility for parsing a particular string.
class TextReaderImpl {
public:
  TextReaderImpl(const char *chars, size_t length, TextReader *parser);

  // Decode a toplevel variant expression. A toplevel expression is different
  // from others because it must fill the whole string.
  bool decode_toplevel(Variant *out);

private:
  // Mapping from ascii characters to the base-64 sextets they represent.
  static const uint8_t kBase64CharToSextet[256];

  // Is there more input to process?
  bool has_more() { return cursor_ < length_; }

  // Returns the current character or \0 if we've read past the end.
  char current() { return has_more() ? chars_[cursor_] : '\0'; }

  // Skips to the next character, returning true iff there is input left.
  bool advance() {
    cursor_++;
    return has_more();
  }

  // Skips to the next character and then advances past any whitespace,
  // returning true iff there is input left.
  bool advance_and_skip() {
    advance();
    skip_whitespace();
    return has_more();
  }

  // Skips of the current and any subsequent whitespace characters.
  void skip_whitespace();

  // Is the given character considered whitespace?
  static bool is_whitespace(char c);

  // Is the given character a newline, that is, a character that terminates an
  // end-of-line comment?
  static bool is_newline(char c);

  // Is the given character a numeric digit?
  static bool is_digit(char c);

  // Fails parsing, returning false.
  bool fail(Variant *out);

  // Succeeds parsing of some expression, returning true.
  bool succeed(Variant value, Variant *out);

  // Decodes a non-toplevel expression.
  bool decode(Variant *out);

  // Parses the next integer.
  bool decode_integer(Variant *out);

  // Parses the next unquoted string.
  bool decode_unquoted_string(Variant *out);

  // Parses the next quoted string.
  bool decode_quoted_string(Variant *out);

  // Parses the next binary blob.
  bool decode_blob(Variant *out);

  // Parses the next array.
  bool decode_array(Variant *out);

  // Parses the next map.
  bool decode_map(Variant *out);

  // Returns the arena to use for allocation.
  Arena *arena() { return parser_->arena_; }

  // Given a character, returns the special character it encodes (for instance
  // a newline for 'n'), or a null character if this one doesn't represent a
  // special character.
  static bool decode_short_escape(char c, char *out);

  size_t length_;
  size_t cursor_;
  const char *chars_;
  TextReader *parser_;
};

TextReaderImpl::TextReaderImpl(const char *chars, size_t length, TextReader *parser)
  : length_(length)
  , cursor_(0)
  , chars_(chars)
  , parser_(parser) {
  skip_whitespace();
}

void TextReaderImpl::skip_whitespace() {
  // Loop around skipping any number of comments and the whitespace between
  // them.
  top: do {
    while (has_more() && is_whitespace(current()))
      advance();
    if (current() == '#') {
      // If there's a comment skip it and then go around one more time.
      while (has_more() && !is_newline(current()))
        advance();
      goto top;
    }
  } while (false);
}

bool TextReaderImpl::is_whitespace(char c) {
  switch (c) {
    case ' ': case '\n': case '\t': case '\f': case '\r':
      return true;
    default:
      return false;
  }
}

bool TextReaderImpl::is_newline(char c) {
  switch (c) {
    case '\n': case '\f':
      return true;
    default:
      return false;
  }
}

bool TextReaderImpl::is_digit(char c) {
  return '0' <= c && c <= '9';
}

bool TextReaderImpl::decode_toplevel(Variant *out) {
  return decode(out) && (!has_more() || fail(out));
}

bool TextReaderImpl::decode(Variant *out) {
  switch (current()) {
    case '%':
      advance();
      switch (current()) {
        case 'f':
          advance_and_skip();
          return succeed(Variant::no(), out);
        case 't':
          advance_and_skip();
          return succeed(Variant::yes(), out);
        case 'n':
          advance_and_skip();
          return succeed(Variant::null(), out);
        case '[':
          return decode_blob(out);
        default:
          return fail(out);
      }
      break;
    case '[':
      return decode_array(out);
    case '{':
      return decode_map(out);
    case '"':
      return decode_quoted_string(out);
    default:
      char c = current();
      if (c == '-' || is_digit(c)) {
        return decode_integer(out);
      } else if (TextWriterImpl::is_unquoted_string_start(c)) {
        return decode_unquoted_string(out);
      } else {
        return fail(out);
      }
  }
}

bool TextReaderImpl::decode_integer(Variant *out) {
  bool is_negative = false;
  if (current() == '-') {
    is_negative = true;
    advance();
  }
  int64_t result = 0;
  while (is_digit(current())) {
    result = (10 * result) + (current() - '0');
    advance();
  }
  skip_whitespace();
  if (is_negative)
    result = -result;
  return succeed(Variant::integer(result), out);
}

bool TextReaderImpl::decode_unquoted_string(Variant *out) {
  const char *start = chars_ + cursor_;
  while (TextWriterImpl::is_unquoted_string_part(current()))
    advance();
  const char *end = chars_ + cursor_;
  skip_whitespace();
  return succeed(arena()->new_string(start, end - start), out);
}

bool TextWriterImpl::encode_short_escape(char c, char *out) {
  switch (c) {
    case '\a': *out = 'a'; break;
    case '\b': *out = 'b'; break;
    case '\f': *out = 'f'; break;
    case '\n': *out = 'n'; break;
    case '\t': *out = 't'; break;
    case '\r': *out = 'r'; break;
    case '\v': *out = 'v'; break;
    case '\0': *out = '0'; break;
    case '\\':
    case '\"':
      *out = c;
      break;
    default:
      return false;
  }
  return true;
}

bool TextReaderImpl::decode_short_escape(char c, char *out) {
  switch (c) {
    case 'a': *out = '\a'; break;
    case 'b': *out = '\b'; break;
    case 'f': *out = '\f'; break;
    case 'n': *out = '\n'; break;
    case 't': *out = '\t'; break;
    case 'r': *out = '\r'; break;
    case 'v': *out = '\v'; break;
    case '0': *out = '\0'; break;
    case '\\':
    case '\"':
      *out = c;
      break;
    default:
      return false;
  }
  return true;
}

static bool parse_hex_digit(char c, uint8_t *out) {
  if ('0' <= c && c <= '9') {
    *out = c - '0';
  } else if ('a' <= c && c <= 'f') {
    *out = c - 'a' + 10;
  } else if ('A' <= c && c <= 'F') {
    *out = c - 'A' + 10;
  } else {
    return false;
  }
  return true;
}

bool TextReaderImpl::decode_quoted_string(Variant *out) {
  advance();
  Buffer<char> buf;
  while (has_more() && current() != '"') {
    if (current() == '\\') {
      if (!advance()) {
        return fail(out);
      } else if (current() == 'x') {
        uint8_t high = 0;
        uint8_t low = 0;
        if (!advance()
         || !parse_hex_digit(current(), &high)
         || !advance()
         || !parse_hex_digit(current(), &low))
          return fail(out);
        buf.add((high << 4) | low);
        advance();
      } else {
        char special = '\0';
        if (!decode_short_escape(current(), &special))
          return fail(out);
        buf.add(special);
        advance();
      }
    } else {
      buf.add(current());
      advance();
    }
  }
  if (current() != '"') {
    return fail(out);
  } else {
    advance_and_skip();
  }
  return succeed(arena()->new_string(*buf, buf.length()), out);
}

bool TextReaderImpl::decode_array(Variant *out) {
  advance_and_skip();
  Array result = arena()->new_array();
  while (has_more() && current() != ']') {
    Variant next;
    if (!decode(&next))
      return fail(out);
    result.add(next);
    if (current() == ',') {
      advance_and_skip();
    } else {
      break;
    }
  }
  if (current() != ']')
    return fail(out);
  advance_and_skip();
  result.ensure_frozen();
  return succeed(result, out);
}

bool TextReaderImpl::decode_map(Variant *out) {
  advance_and_skip();
  Map result = arena()->new_map();
  while (has_more() && current() != '}') {
    Variant key;
    if (!decode(&key))
      return fail(out);
    if (current() != ':')
      return fail(out);
    advance_and_skip();
    Variant value;
    if (!decode(&value))
      return fail(out);
    result.set(key, value);
    if (current() == ',') {
      advance_and_skip();
    } else {
      break;
    }
  }
  if (current() != '}')
    return fail(out);
  advance_and_skip();
  result.ensure_frozen();
  return succeed(result, out);
}

// An invalid sextet.
#define INV 255

// Padding marker.
#define PAD 254

const uint8_t TextReaderImpl::kBase64CharToSextet[256] = {
  INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV,
  INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV,
  INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, 62,  INV, INV, INV, 63,
  52,  53,  54,  55,  56,  57,  58,  59,  60,  61,  INV, INV, INV, PAD, INV, INV,
  INV, 0,   1,   2,   3,   4,   5,   6,   7,   8,   9,   10,  11,  12,  13,  14,
  15,  16,  17,  18,  19,  20,  21,  22,  23,  24,  25,  INV, INV, INV, INV, INV,
  INV, 26,  27,  28,  29,  30,  31,  32,  33,  34,  35,  36,  37,  38,  39,  40,
  41,  42,  43,  44,  45,  46,  47,  48,  49,  50,  51,  INV, INV, INV, INV, INV,
  INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV,
  INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV,
  INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV,
  INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV,
  INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV,
  INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV,
  INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV,
  INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV
};

bool TextReaderImpl::decode_blob(Variant *out) {
  Buffer<uint8_t> data;
  if (current() == '[') {
    advance_and_skip();
  } else {
    return fail(out);
  }
  while (has_more() && current() != ']') {
    // Read the next block of 4 characters. Padding is mandatory so we always
    // read blocks of 4 at a time regardless of their contents.
    char ac = current();
    uint8_t a = kBase64CharToSextet[static_cast<uint8_t>(ac)];
    if (a == INV || a == PAD || !advance_and_skip())
      return fail(out);
    char bc = current();
    uint8_t b = kBase64CharToSextet[static_cast<uint8_t>(bc)];
    if (b == INV || b == PAD || !advance_and_skip())
      return fail(out);
    char cc = current();
    uint8_t c = kBase64CharToSextet[static_cast<uint8_t>(cc)];
    if (c == INV || !advance_and_skip())
      return fail(out);
    char dc = current();
    uint8_t d = kBase64CharToSextet[static_cast<uint8_t>(dc)];
    if (d == INV || !advance_and_skip())
      return fail(out);
    // Then decode the values.
    data.add((a << 2) | (b >> 4));
    if (c != PAD) {
      data.add(((b << 4) | (c >> 2)) & 0xFF);
      if (d != PAD)
        data.add(((c << 6) | d) & 0xFF);
    }
  }
  if (current() == ']') {
    advance_and_skip();
    Variant result = arena()->new_blob(*data, data.length());
    return succeed(result, out);
  } else {
    return fail(out);
  }
}

bool TextReaderImpl::fail(Variant *out) {
  parser_->has_failed_ = true;
  parser_->offender_ = current();
  *out = Variant::null();
  return false;
}

bool TextReaderImpl::succeed(Variant value, Variant *out) {
  *out = value;
  return true;
}

TextReader::TextReader(Arena *arena)
  : arena_(arena)
  , has_failed_(false)
  , offender_('\0') { }

Variant TextReader::parse(const char *chars, size_t length) {
  has_failed_ = false;
  offender_ = '\0';
  TextReaderImpl decoder(chars, length, this);
  Variant result;
  decoder.decode_toplevel(&result);
  return result;
}

} // namespace plankton
