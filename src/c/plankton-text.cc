//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "c/stdc.h"
#include "c/stdnew.hh"
#include "marshal-inl.hh"
#include "plankton-inl.hh"
#include "utils-inl.hh"

BEGIN_C_INCLUDES
#include "utils/check.h"
END_C_INCLUDES

namespace plankton {

TextWriter::TextWriter(TextSyntax syntax)
  : syntax_(syntax)
  , chars_(NULL)
  , length_(0) { }

TextWriter::~TextWriter() {
  delete[] chars_;
  chars_ = NULL;
}

// Utility used to print plankton values as ascii.
class TextWriterImpl {
public:
  TextWriterImpl() { }
  virtual ~TextWriterImpl() { }

  // Writes the given value on the stream at the current indentation level.
  void write(Variant value);

  // Null-terminates and stores the result in the destination.
  void flush(TextWriter *writer);

  // Is the given character allowed as the first character of a string to be
  // written?
  static bool is_unquoted_string_start(char c);

  // Is the given character allowed as a subsequent character of an unquoted
  // string to be written?
  static bool is_unquoted_string_part(char c);

protected:
  // Writes any pending newlines.
  virtual void flush_pending_newline() = 0;

  // Schedules a newline to be printed before the next non-newline character.
  virtual void schedule_newline() = 0;

  // Increases the indentation level.
  virtual void indent() = 0;

  // Decreases the indentation level.
  virtual void deindent() = 0;

  // Writes an array value.
  virtual void write_array(Array array) = 0;

  // Writes a map value.
  virtual void write_map(Map map) = 0;

  // Writes a seed value.
  virtual void write_seed(Seed seed) = 0;

  // Writes the given character directly to the buffer.
  void write_raw_char(char c);

  Buffer<char> chars_;

private:
  // Writes the given null-terminated string directly to the buffer.
  void write_raw_string(const char *chars);

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

  // Writes a native value.
  void write_native(Native obj);

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

  Arena scratch_;
};

class SourceTextWriterImpl : public TextWriterImpl {
public:
  SourceTextWriterImpl();

protected:
  virtual void flush_pending_newline();
  virtual void schedule_newline();
  virtual void indent();
  virtual void deindent();
  virtual void write_array(Array array);
  virtual void write_map(Map map);
  virtual void write_seed(Seed map);

private:
  // Lengths up to (but not including) this will be considered short. Longer
  // lengths are considered, for all practical purposes, infinite.
  static const size_t kShortLengthLimit = 80;

  // Returns the length of the given variant, added to the given offset. The
  // short length limit is treated as infinity so if we ever reach a value
  // greater we bail out immediately. This is to keep the calculation constant
  // and avoid the potential complexity blowup if we compute the full size
  // of subtrees for every variant.
  static size_t get_short_length(Variant variant, size_t offset);

  // Will writing the given value cause the line to get too long so we have to
  // use the long multiline format?
  bool write_long(Variant value);

  size_t indent_;
  bool has_pending_newline_;
};

class CommandTextWriterImpl : public TextWriterImpl {
protected:
  virtual void flush_pending_newline() { }
  virtual void schedule_newline() { }
  virtual void indent() { }
  virtual void deindent() { }
  virtual void write_array(Array array);
  virtual void write_map(Map map);
  virtual void write_seed(Seed map);
};

SourceTextWriterImpl::SourceTextWriterImpl()
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
      write_array(value);
      break;
    case PTON_MAP:
      write_map(value);
      break;
    case PTON_SEED:
      write_seed(value);
      break;
    case PTON_NATIVE:
      write_native(value);
      break;
    default:
      write_raw_string("?");
      break;
  }
}

size_t SourceTextWriterImpl::get_short_length(Variant value, size_t offset) {
  switch (value.type()) {
    case PTON_INTEGER:
      return offset + 5;
    case PTON_BOOL:
    case PTON_NULL:
      return offset + 2;
    case PTON_STRING:
      return offset + value.string_length();
    case PTON_ARRAY: {
      Array array = value;
      size_t current = offset + 2;
      for (size_t i = 0; i < array.length() && current < kShortLengthLimit; i++)
        current = get_short_length(array[i], current + 2);
      return current;
    }
    case PTON_MAP: {
      Map map = value;
      size_t current = offset + 2;
      for (Map::Iterator i = map.begin();
           i != map.end() && current < kShortLengthLimit;
           i++) {
        current = get_short_length(i->key(), current + 2);
        current = get_short_length(i->value(), current);
      }
      return current;
    }
    case PTON_SEED: {
      Seed seed = value;
      size_t current = get_short_length(seed.header(), offset + 2);
      for (Seed::Iterator i = seed.fields_begin();
           i != seed.fields_end() && current < kShortLengthLimit;
           i++) {
        current = get_short_length(i->key(), current + 3);
        current = get_short_length(i->value(), current);
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

void SourceTextWriterImpl::flush_pending_newline() {
  if (!has_pending_newline_)
    return;
  chars_.add('\n');
  chars_.fill(' ', indent_);
  has_pending_newline_ = false;
}

void SourceTextWriterImpl::schedule_newline() {
  has_pending_newline_ = true;
}

void SourceTextWriterImpl::indent() {
  indent_ += 2;
}

void SourceTextWriterImpl::deindent() {
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

// Characters that are allowed in unquoted strings, beyond the ascii alnum ones.
//
// TODO: How to generalize this to non-latin characters without requiring the
//   massive full weight of ICU or something similar?
static const char *kUnquotedStringSpecials = "_-/.";

bool TextWriterImpl::is_unquoted_string_part(char c) {
  return is_unquoted_string_start(c)
      || ('0' <= c && c <= '9')
      || ((c != '\0') && (strchr(kUnquotedStringSpecials, c) != NULL));
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

bool SourceTextWriterImpl::write_long(Variant value) {
  return get_short_length(value, indent_) >= kShortLengthLimit;
}

void SourceTextWriterImpl::write_array(Array array) {
  bool is_long = write_long(array);
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

void CommandTextWriterImpl::write_array(Array array) {
  write_raw_char('[');
  for (size_t i = 0; i < array.length(); i++) {
    Variant value = array[i];
    write(value);
    if (i + 1 < array.length())
      write_raw_char(' ');
  }
  write_raw_char(']');
}

void SourceTextWriterImpl::write_map(Map map) {
  bool is_long = write_long(map);
  write_raw_char('{');
  if (is_long) {
    indent();
    schedule_newline();
  }
  for (Map::Iterator i = map.begin(); i != map.end(); i++) {
    write(i->key());
    write_raw_char(':');
    write_raw_char(' ');
    write(i->value());
    if (i.has_next()) {
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

void CommandTextWriterImpl::write_map(Map map) {
  write_raw_char('{');
  for (Map::Iterator i = map.begin(); i != map.end(); i++) {
    write_raw_char('-');
    write_raw_char('-');
    write(i->key());
    write_raw_char(' ');
    write(i->value());
    if (i.has_next())
      write_raw_char(' ');
  }
  write_raw_char('}');
}

void SourceTextWriterImpl::write_seed(Seed seed) {
  bool is_long = write_long(seed);
  write_raw_char('@');
  write(seed.header());
  write_raw_char(is_long ? '{' : '(');
  if (is_long) {
    indent();
    schedule_newline();
  }
  for (Seed::Iterator i = seed.fields_begin(); i != seed.fields_end(); i++) {
    write(i->key());
    write_raw_char(':');
    write_raw_char(' ');
    write(i->value());
    if (i.has_next()) {
      write_raw_char(',');
      if (!is_long)
        write_raw_char(' ');
    }
    if (is_long)
      schedule_newline();
  }
  if (is_long)
    deindent();
  write_raw_char(is_long ? '}' : ')');
}

void CommandTextWriterImpl::write_seed(Seed seed) {
  write_raw_char('@');
  write(seed.header());
  write_raw_char('(');
  for (Seed::Iterator i = seed.fields_begin(); i != seed.fields_end(); i++) {
    write_raw_char('-');
    write_raw_char('-');
    write(i->key());
    write_raw_char(' ');
    write(i->value());
    if (i.has_next())
      write_raw_char(' ');
  }
  write_raw_char(')');
}

void TextWriterImpl::write_native(Native value) {
  AbstractSeedType *type = value.type();
  Variant replacement = type->encode_instance(value, &scratch_);
  write(replacement);
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
  if (syntax_ == SOURCE_SYNTAX) {
    SourceTextWriterImpl impl;
    impl.write(value);
    impl.flush(this);
  } else {
    CHECK_EQ("unexpected syntax", COMMAND_SYNTAX, syntax_);
    CommandTextWriterImpl impl;
    impl.write(value);
    impl.flush(this);
  }
}

// Utility for parsing a particular string.
class TextReaderImpl {
public:
  TextReaderImpl(const char *chars, size_t length, TextReader *parser);
  virtual ~TextReaderImpl() { }

  // Decode a full variant expression. A full expression is different from
  // others because it must fill the whole string.
  bool decode_full(Variant *out);

  bool decode_command_line_full(Variant *out);

  // Is the given character allowed as the first character of a string to be
  // read?
  static bool is_unquoted_string_start(char c);

  // Is the given character allowed as a subsequent character of an unquoted
  // string to be read?
  static bool is_unquoted_string_part(char c);

protected:
  // Mapping from ascii characters to the base-64 sextets they represent.
  static const uint8_t kBase64CharToSextet[256];

  // Is there more input to process?
  bool has_more() { return cursor_ < length_; }

  // Returns the current character or \0 if we've read past the end.
  char current() { return has_more() ? chars_[cursor_] : '\0'; }

  // Returns the next character or \0 if it's past the end.
  char next() { return cursor_ + 1 < length_ ? chars_[cursor_ + 1] : '\0'; }

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

  // Utility used by skip_whitespace to skip block comments. When called it is
  // assumed that we've just seen a #.
  void skip_comments();

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

  // Decodes a single, possibly escaped, character in a string, quoted or not.
  bool decode_character(char *out);

  // Parses the next quoted string.
  bool decode_quoted_string(Variant *out);

  // Parses the next binary blob.
  bool decode_blob(Variant *out);

  // Parses the next array.
  virtual bool decode_array(Variant *out) = 0;

  // Parses the next map.
  virtual bool decode_map(Variant *out) = 0;

  // Parses the next seed.
  virtual bool decode_seed(Variant *out) = 0;

  virtual bool decode_command_line(Variant *out) = 0;

  // Returns the factory to use for allocation.
  Factory *factory() { return parser_->factory_; }

private:
  // Given a character, returns the special character it encodes (for instance
  // a newline for 'n'), or a null character if this one doesn't represent a
  // special character.
  static bool decode_short_escape(char c, char *out);

  size_t length_;
  size_t cursor_;
  const char *chars_;
  TextReader *parser_;
};

class SourceTextReaderImpl : public TextReaderImpl {
public:
  SourceTextReaderImpl(const char *chars, size_t length, TextReader *parser)
    : TextReaderImpl(chars, length, parser) { }

protected:
  virtual bool decode_array(Variant *out);
  virtual bool decode_map(Variant *out);
  virtual bool decode_seed(Variant *out);
  virtual bool decode_command_line(Variant *out) { return fail(out); }
};

class CommandTextReaderImpl : public TextReaderImpl {
public:
  CommandTextReaderImpl(const char *chars, size_t length, TextReader *parser)
    : TextReaderImpl(chars, length, parser) { }

protected:
  virtual bool decode_array(Variant *out);
  virtual bool decode_map(Variant *out);
  virtual bool decode_seed(Variant *out);
  virtual bool decode_command_line(Variant *out);
};

TextReaderImpl::TextReaderImpl(const char *chars, size_t length, TextReader *parser)
  : length_(length)
  , cursor_(0)
  , chars_(chars)
  , parser_(parser) {
  skip_whitespace();
}

void TextReaderImpl::skip_whitespace() {
  while (true) {
    while (has_more() && is_whitespace(current()))
      advance();
    if (current() == '#') {
      // If we see a comment skip it and go around again to skip any whitespace
      // following the comment.
      advance();
      skip_comments();
    } else {
      // Not a comment and not whitespace -- we're done.
      break;
    }
  }
}

void TextReaderImpl::skip_comments() {
  if (current() == '{') {
    // Block comment; just skip until we see a # which may or may not end this
    // block.
    while (true) {
      while (has_more() && current() != '#')
        advance();
      if (current() != '#')
        // Ran out of input; bail out.
        break;
      advance();
      if (current() == '}') {
        // Found the comment end marker so we're done.
        advance();
        break;
      } else {
        // Found some other kind of comment marker. Skip it. Note that this
        // means that if the block end marker is within a nested EOL comment it
        // will be counted as commented out and not used to end the block.
        // Basically, EOL comments are considered to have higher precedence than
        // block comments.
        skip_comments();
        // Since this wasn't the end marker we loop around and keep looking.
        continue;
      }
    }
  } else {
    // This is not a block comment so it must be an EOL one. Skip it.
    while (has_more() && !is_newline(current()))
      advance();
  }
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

bool TextReaderImpl::is_unquoted_string_start(char c) {
  return TextWriterImpl::is_unquoted_string_start(c) || (c == '\\');
}

bool TextReaderImpl::is_unquoted_string_part(char c) {
  return TextWriterImpl::is_unquoted_string_part(c) || (c == '\\');
}

bool TextReaderImpl::is_digit(char c) {
  return '0' <= c && c <= '9';
}

bool TextReaderImpl::decode_full(Variant *out) {
  return decode(out) && (!has_more() || fail(out));
}

bool TextReaderImpl::decode_command_line_full(Variant *out) {
  return decode_command_line(out);
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
    case '@':
      return decode_seed(out);
    case '"':
      return decode_quoted_string(out);
    case '-':
      return (next() == '-' ? fail(out) : decode_integer(out));
    default:
      char c = current();
      if (is_digit(c)) {
        return decode_integer(out);
      } else if (is_unquoted_string_start(c)) {
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

bool TextReaderImpl::decode_unquoted_string(Variant *out) {
  Buffer<char> buf;
  while (has_more() && is_unquoted_string_part(current())) {
    char next = '\0';
    if (!decode_character(&next))
      return fail(out);
    buf.add(next);
  }
  skip_whitespace();
  return succeed(factory()->new_string(*buf, buf.length()), out);
}

bool TextReaderImpl::decode_character(char *out) {
  if (current() == '\\') {
    if (!advance()) {
      return false;
    } else if (current() == 'x') {
      uint8_t high = 0;
      uint8_t low = 0;
      if (!advance()
       || !parse_hex_digit(current(), &high)
       || !advance()
       || !parse_hex_digit(current(), &low))
        return false;
      *out = (high << 4) | low;
      advance();
    } else {
      if (!decode_short_escape(current(), out))
        return false;
      advance();
    }
  } else {
    *out = current();
    advance();
  }
  return true;
}

bool TextReaderImpl::decode_quoted_string(Variant *out) {
  advance();
  Buffer<char> buf;
  while (has_more() && current() != '"') {
    char next = '\0';
    if (!decode_character(&next))
      return fail(out);
    buf.add(next);
  }
  if (current() != '"') {
    return fail(out);
  } else {
    advance_and_skip();
  }
  return succeed(factory()->new_string(*buf, buf.length()), out);
}

bool SourceTextReaderImpl::decode_array(Variant *out) {
  advance_and_skip();
  Array result = factory()->new_array();
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

bool CommandTextReaderImpl::decode_array(Variant *out) {
  advance_and_skip();
  Array result = factory()->new_array();
  while (has_more() && current() != ']') {
    Variant next;
    if (!decode(&next))
      return fail(out);
    result.add(next);
  }
  if (current() != ']')
    return fail(out);
  advance_and_skip();
  result.ensure_frozen();
  return succeed(result, out);
}

bool SourceTextReaderImpl::decode_map(Variant *out) {
  advance_and_skip();
  Map result = factory()->new_map();
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

bool CommandTextReaderImpl::decode_map(Variant *out) {
  advance_and_skip();
  Map result = factory()->new_map();
  while (has_more() && current() != '}') {
    if (current() != '-')
      return fail(out);
    advance();
    if (current() != '-')
      return fail(out);
    advance_and_skip();
    Variant key;
    if (!decode(&key))
      return fail(out);
    Variant value;
    if (!decode(&value))
      return fail(out);
    result.set(key, value);
  }
  if (current() != '}')
    return fail(out);
  advance_and_skip();
  result.ensure_frozen();
  return succeed(result, out);
}

bool SourceTextReaderImpl::decode_seed(Variant *out) {
  advance_and_skip();
  Variant header;
  if (!decode(&header))
    return fail(out);
  char end;
  if (current() == '(') {
    end = ')';
  } else if (current() == '{') {
    end = '}';
  } else {
    return fail(out);
  }
  advance_and_skip();
  Seed result = factory()->new_seed();
  result.set_header(header);
  while (has_more() && current() != end) {
    Variant key;
    if (!decode(&key))
      return fail(out);
    if (current() != ':')
      return fail(out);
    advance_and_skip();
    Variant value;
    if (!decode(&value))
      return fail(out);
    result.set_field(key, value);
    if (current() == ',') {
      advance_and_skip();
    } else {
      break;
    }
  }
  if (current() != end)
    return fail(out);
  advance_and_skip();
  result.ensure_frozen();
  return succeed(result, out);
}

bool CommandTextReaderImpl::decode_seed(Variant *out) {
  advance_and_skip();
  Variant header;
  if (!decode(&header))
    return fail(out);
  char end;
  if (current() == '(') {
    end = ')';
  } else if (current() == '{') {
    end = '}';
  } else {
    return fail(out);
  }
  advance_and_skip();
  Seed result = factory()->new_seed();
  result.set_header(header);
  while (has_more() && current() != end) {
    if (current() != '-' || next() != '-')
      return fail(out);
    advance();
    advance_and_skip();
    Variant key;
    if (!decode(&key))
      return fail(out);
    Variant value;
    if (!decode(&value))
      return fail(out);
    result.set_field(key, value);
  }
  if (current() != end)
    return fail(out);
  advance_and_skip();
  result.ensure_frozen();
  return succeed(result, out);
}

bool CommandTextReaderImpl::decode_command_line(Variant *out) {
  Array args = factory()->new_array();
  Map options = factory()->new_map();
  while (has_more()) {
    if (current() == '-' && next() == '-') {
      advance();
      advance_and_skip();
      Variant key;
      if (!decode(&key))
        return fail(out);
      Variant value;
      if (!decode(&value))
        return fail(out);
      options.set(key, value);
    } else {
      Variant arg;
      if (!decode(&arg))
        return fail(out);
      args.add(arg);
    }
  }
  CommandLine *cmdline = new (factory()) CommandLine(args, options);
  Native result = factory()->new_native(cmdline);
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
    Variant result = factory()->new_blob(*data, data.length());
    return succeed(result, out);
  } else {
    return fail(out);
  }
}

SeedType<SyntaxError> SyntaxError::kSeedType("plankton.SyntaxError");

bool TextReaderImpl::fail(Variant *out) {
  // The ownership of the input isn't tied to the factory the syntax error comes
  // from so we need to copy it there so it'll stay alive while the syntax error
  // is alive.
  String source_copy = factory()->new_string(chars_, strlen(chars_));
  SyntaxError *error = new (factory()) SyntaxError(source_copy, cursor_);
  parser_->error_ = error;
  *out = factory()->new_native(error);
  return false;
}

bool TextReaderImpl::succeed(Variant value, Variant *out) {
  *out = value;
  return true;
}

TextReader::TextReader(TextSyntax syntax, Factory *factory)
  : factory_(factory)
  , scratch_arena_(NULL)
  , syntax_(syntax)
  , error_(NULL) {
  if (factory_ == NULL) {
    scratch_arena_ = new Arena();
    factory_ = scratch_arena_;
  }
}

TextReader::~TextReader() {
  delete scratch_arena_;
}

Variant TextReader::parse(const char *chars, size_t length) {
  error_ = NULL;
  Variant result;
  if (syntax_ == SOURCE_SYNTAX) {
    SourceTextReaderImpl decoder(chars, length, this);
    decoder.decode_full(&result);
  } else {
    CHECK_EQ("unexpected syntax", COMMAND_SYNTAX, syntax_);
    CommandTextReaderImpl decoder(chars, length, this);
    decoder.decode_full(&result);
  }
  return result;
}

SeedType<CommandLine> CommandLine::kSeedType("CommandLine");

CommandLine *CommandLineReader::parse(const char *chars, size_t length) {
  error_ = NULL;
  Variant result;
  CommandTextReaderImpl decoder(chars, length, this);
  if (!decoder.decode_command_line_full(&result)) {
    CHECK_TRUE("no error object", error_ != NULL);
    return new (factory_) CommandLine(error_);
  } else {
    return native_cast<CommandLine>(result);
  }
}

CommandLine *CommandLineReader::parse(int argc, const char **argv) {
  int length = 0;
  char *joined = join_argv(argc, argv, &length);
  CommandLine *result = parse(joined, length);
  delete[] joined;
  return result;
}

char *CommandLineReader::join_argv(int argc, const char **argv, int *len_out) {
  size_t length;
  if (argc == 0) {
    length = 0;
  } else {
    length = (argc - 1);
    for (int i = 0; i < argc; i++)
      length += strlen(argv[i]);
  }
  char *result = new char[length + 1];
  result[length] = '\0';
  char *ptr = result;
  for (int i = 0; i < argc; i++) {
    if (i > 0)
      *(ptr++) = ' ';
    size_t arglen = strlen(argv[i]);
    memcpy(ptr, argv[i], arglen * sizeof(char));
    ptr += arglen;
  }
  *len_out = length;
  return result;
}


Variant CommandLine::option(Variant field, Variant defawlt) {
  return options_.has(field) ? options_[field] : defawlt;
}

} // namespace plankton

pton_command_line_reader_t *pton_new_command_line_reader() {
  return new plankton::CommandLineReader();
}

void pton_dispose_command_line_reader(pton_command_line_reader_t *that) {
  delete static_cast<plankton::CommandLineReader*>(that);
}

pton_command_line_t *pton_command_line_reader_parse(
    pton_command_line_reader_t *reader, int argc, const char **argv) {
  return static_cast<plankton::CommandLineReader*>(reader)->parse(argc, argv);
}

size_t pton_command_line_argument_count(pton_command_line_t *that) {
  return static_cast<plankton::CommandLine*>(that)->argument_count();
}

pton_variant_t pton_command_line_argument(pton_command_line_t *that, size_t i) {
  return static_cast<plankton::CommandLine*>(that)->argument(i).to_c();
}

size_t pton_command_line_option_count(pton_command_line_t *that) {
  return static_cast<plankton::CommandLine*>(that)->option_count();
}

pton_variant_t pton_command_line_option(pton_command_line_t *that,
    pton_variant_t key, pton_variant_t defawlt) {
  return static_cast<plankton::CommandLine*>(that)->option(key, defawlt).to_c();
}

bool pton_command_line_is_valid(pton_command_line_t *that) {
  return static_cast<plankton::CommandLine*>(that)->is_valid();
}

pton_syntax_error_t *pton_command_line_error(pton_command_line_t *that) {
  return static_cast<plankton::CommandLine*>(that)->error();
}

char pton_syntax_error_offender(pton_syntax_error_t *that) {
  return static_cast<plankton::SyntaxError*>(that)->offender();
}
