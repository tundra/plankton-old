//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "plankton-inl.hh"
#include "stdc.h"
#include <new>

namespace plankton {

// An arena-allocated array.
class arena_array_t {
public:
  explicit arena_array_t(arena_t *origin);

  // Appends the given element to this array.
  arena_array_t *add(variant_t value);

  variant_t operator[](size_t index);

  size_t length() { return length_; }

private:
  arena_t *origin_;
  size_t length_;
  size_t capacity_;
  variant_t *elms_;
};

class arena_map_t {
public:
  struct entry_t {
    variant_t key;
    variant_t value;
  };

  explicit arena_map_t(arena_t *origin);

  arena_map_t *set(variant_t key, variant_t value);

  variant_t operator[](variant_t key);

  size_t size() { return size_; }

private:
  friend class map_t::iterator;

  arena_t *origin_;
  size_t size_;
  size_t capacity_;
  entry_t *elms_;
};

class arena_string_t {
public:
  explicit arena_string_t(const char *chars, size_t length);

  size_t length() { return length_; }

  const char *chars() { return chars_; }

private:
  const char *chars_;
  size_t length_;
};

class arena_blob_t {
public:
  explicit arena_blob_t(const void *data, size_t size);

  size_t size() { return size_; }

  const void *data() { return data_; }

private:
  const void *data_;
  size_t size_;
};

class arena_sink_t {
public:
  explicit arena_sink_t(arena_t *origin);

  bool set(variant_t value);

  variant_t operator*();

private:
  bool is_empty_;
  variant_t value_;
  arena_t *origin_;
};

void *arena_t::alloc_raw(size_t bytes) {
  if (used_ == capacity_) {
    // Expand capacity if necessary. This is also where we start since the
    // initial used and capacity are 0.
    capacity_ = ((capacity_ < 16) ? 16 : (capacity_ * 2));
    uint8_t **new_blocks = new uint8_t*[capacity_];
    memcpy(new_blocks, blocks_, sizeof(uint8_t*) * used_);
    delete[] blocks_;
    blocks_ = new_blocks;
  }
  uint8_t *result = new uint8_t[bytes];
  blocks_[used_++] = result;
  return result;
}

arena_t::~arena_t() {
  for (size_t i = 0; i < used_; i++)
    delete[] blocks_[i];
  delete[] blocks_;
}

array_t arena_t::new_array() {
  arena_array_t *data = alloc_value<arena_array_t>();
  return array_t(new (data) arena_array_t(this));
}

map_t arena_t::new_map() {
  arena_map_t *data = alloc_value<arena_map_t>();
  return map_t(new (data) arena_map_t(this));
}

variant_t arena_t::new_string(const char *str) {
  return new_string(str, strlen(str));
}

variant_t arena_t::new_string(const char *str, size_t length) {
  arena_string_t *data = alloc_value<arena_string_t>();
  char *own_str = alloc_values<char>(length + 1);
  memcpy(own_str, str, length);
  own_str[length] = '\0';
  return variant_t(new (data) arena_string_t(own_str, length));
}

variant_t arena_t::new_blob(const void *start, size_t size) {
  arena_blob_t *data = alloc_value<arena_blob_t>();
  uint8_t *own_start = alloc_values<uint8_t>(size);
  memcpy(own_start, start, size);
  return variant_t(new (data) arena_blob_t(own_start, size));
}

sink_t arena_t::new_sink() {
  arena_sink_t *data = alloc_sink();
  return sink_t(data);
}

arena_sink_t *arena_t::alloc_sink() {
  arena_sink_t *result = alloc_value<arena_sink_t>();
  return new (result) arena_sink_t(this);
}

variant_t::type_t variant_t::type() const {
  return static_cast<type_t>(repr_tag_ >> 4);
}

bool variant_t::operator==(variant_t that) {
  type_t this_type = type();
  type_t that_type = that.type();
  if (this_type != that_type)
    return false;
  switch (this_type) {
    case vtInteger:
      return integer_value() == that.integer_value();
    case vtString: {
      size_t length = string_length();
      if (that.string_length() != length)
        return false;
      return strncmp(string_chars(), that.string_chars(), length) == 0;
    }
    case vtBlob: {
      size_t size = blob_size();
      if (that.blob_size() != size)
        return false;
      return strncmp(static_cast<const char*>(blob_data()),
          static_cast<const char*>(that.blob_data()), size) == 0;
    }
    case vtArray:
      return data_.as_arena_array_ == that.data_.as_arena_array_;
    case vtMap:
      return data_.as_arena_map_ == that.data_.as_arena_map_;
    case vtNull: case vtTrue: case vtFalse:
      return true;
    default:
      return false;
  }
}

variant_t variant_t::blob(const void *data, size_t size) {
  variant_t result;
  result.repr_tag_ = rtExternalBlob;
  result.data_.as_external_blob_.data_ = data;
  result.data_.as_external_blob_.size_ = size;
  return result;
}

uint32_t variant_t::blob_size() const {
  switch (repr_tag_) {
    case rtExternalBlob:
      return data_.as_external_blob_.size_;
    case rtArenaBlob:
      return data_.as_arena_blob_->size();
    default:
      return 0;
  }
}

const void *variant_t::blob_data() const {
  switch (repr_tag_) {
    case rtExternalBlob:
      return data_.as_external_blob_.data_;
    case rtArenaBlob:
      return data_.as_arena_blob_->data();
    default:
      return NULL;
  }
}

array_t::array_t(arena_array_t *data) : variant_t(data) { }

array_t::array_t(variant_t variant) : variant_t() {
  // Initialize this to the null value and then, if the given variant is an
  // array, override with the variant's state.
  if (variant.type() == vtArray)
    *static_cast<variant_t*>(this) = variant;
}

array_t array_t::add(variant_t value) {
  if (*this)
    data()->add(value);
  return *this;
}

size_t array_t::length() {
  return *this ? data()->length() : 0;
}

variant_t array_t::operator[](size_t index) {
  return *this ? data()->operator[](index) : null();
}

arena_array_t *array_t::data() {
  return data_.as_arena_array_;
}

arena_array_t::arena_array_t(arena_t *origin)
  : origin_(origin)
  , length_(0)
  , capacity_(0)
  , elms_(NULL) { }

arena_array_t *arena_array_t::add(variant_t value) {
  if (length_ == capacity_) {
    capacity_ = (capacity_ < 4 ? 4 : (2 * capacity_));
    variant_t *new_elms = origin_->alloc_values<variant_t>(capacity_);
    memcpy(new_elms, elms_, sizeof(variant_t) * length_);
    elms_ = new_elms;
  }
  elms_[length_++] = value;
  return this;
}

variant_t arena_array_t::operator[](size_t index) {
  return (index < length_) ? elms_[index] : variant_t::null();
}

map_t::map_t(arena_map_t *data) : variant_t(data) { }

map_t::map_t(variant_t variant) : variant_t() {
  // Initialize this to the null value and then, if the given variant is a map,
  // override with the variant's state.
  if (variant.type() == vtMap)
    *static_cast<variant_t*>(this) = variant;
}

size_t map_t::size() {
  return *this ? data()->size() : 0;
}

map_t map_t::set(variant_t key, variant_t value) {
  if (*this)
    data()->set(key, value);
  return *this;
}

variant_t map_t::operator[](variant_t key) {
  return *this ? data()->operator[](key) : variant_t::null();
}

arena_map_t *map_t::data() {
  return data_.as_arena_map_;
}

bool map_t::iterator::advance(variant_t *key, variant_t *value) {
  if (cursor_ == data_->size()) {
    return false;
  } else {
    *key = data_->elms_[cursor_].key;
    *value = data_->elms_[cursor_].value;
    cursor_++;
    return true;
  }
}

bool map_t::iterator::has_next() {
  return cursor_ < data_->size();
}

arena_map_t::arena_map_t(arena_t *origin)
  : origin_(origin)
  , size_(0)
  , capacity_(0)
  , elms_(NULL) { }

arena_map_t *arena_map_t::set(variant_t key, variant_t value) {
  if (size_ == capacity_) {
    capacity_ = (capacity_ < 4 ? 4 : (2 * capacity_));
    entry_t *new_elms = origin_->alloc_values<entry_t>(capacity_);
    memcpy(new_elms, elms_, sizeof(entry_t) * size_);
    elms_ = new_elms;
  }
  entry_t *entry = &elms_[size_++];
  entry->key = key;
  entry->value = value;
  return this;
}

variant_t arena_map_t::operator[](variant_t key) {
  for (size_t i = 0; i < size_; i++) {
    entry_t *entry = &elms_[i];
    if (entry->key == key)
      return entry->value;
  }
  return variant_t::null();
}

uint32_t variant_t::string_length() const {
  switch (repr_tag_) {
    case rtExternalString:
      return data_.as_external_string_.length_;
    case rtArenaString:
      return data_.as_arena_string_->length();
    default:
      return 0;
  }
}

const char *variant_t::string_chars() const {
  switch (repr_tag_) {
    case rtExternalString:
      return data_.as_external_string_.chars_;
    case rtArenaString:
      return data_.as_arena_string_->chars();
    default:
      return NULL;
  }
}

arena_string_t::arena_string_t(const char *chars, size_t length)
  : chars_(chars)
  , length_(length) { }

arena_blob_t::arena_blob_t(const void *data, size_t size)
  : data_(data)
  , size_(size) { }

sink_t::sink_t(arena_sink_t *data)
  : data_(data) { }

variant_t sink_t::operator*() const {
  return data_->operator*();
}

bool sink_t::set(variant_t value) {
  return data_->set(value);
}

arena_sink_t::arena_sink_t(arena_t *origin)
  : is_empty_(true)
  , origin_(origin) { }

variant_t arena_sink_t::operator*() {
  return value_;
}

bool arena_sink_t::set(variant_t value) {
  if (is_empty_) {
    is_empty_ = false;
    value_ = value;
    return true;
  } else {
    return false;
  }
}

template <typename T>
class Buffer {
public:
  Buffer();
  ~Buffer();

  void add(const T &value);

  void write(const T *data, size_t count);

  void fill(const T &value, size_t count);

  T *operator*() { return data_; }

  size_t length() { return cursor_; }

  // Returns the contents of this buffer and then removes any references to it
  // such that it will not be disposed when this buffer is destroyed.
  T *release();

private:
  void ensure_capacity(size_t size);

  size_t capacity_;
  size_t cursor_;
  T *data_;
};

template <typename T>
Buffer<T>::Buffer()
  : capacity_(0)
  , cursor_(0)
  , data_(NULL) { }

template <typename T>
Buffer<T>::~Buffer() {
  delete[] data_;
  data_ = NULL;
}

template <typename T>
void Buffer<T>::add(const T &value) {
  ensure_capacity(1);
  data_[cursor_++] = value;
}

template <typename T>
void Buffer<T>::fill(const T &value, size_t count) {
  ensure_capacity(count);
  for (size_t i = 0; i < count; i++)
    data_[cursor_++] = value;
}

template <typename T>
void Buffer<T>::write(const T *data, size_t count) {
  ensure_capacity(count);
  memcpy(data_ + cursor_, data, count * sizeof(T));
  cursor_ += count;
}

template <typename T>
void Buffer<T>::ensure_capacity(size_t size) {
  size_t required = cursor_ + size;
  if (required < capacity_)
    return;
  size_t new_capacity = (required < 128) ? 256 : (2 * required);
  T *new_data = new T[new_capacity];
  if (cursor_ > 0)
    memcpy(new_data, data_, sizeof(T) * cursor_);
  delete[] data_;
  data_ = new_data;
  capacity_ = new_capacity;
}

template <typename T>
T *Buffer<T>::release() {
  T *result = data_;
  data_ = NULL;
  return result;
}

TextWriter::TextWriter()
  : chars_(NULL)
  , length_(0) { }

TextWriter::~TextWriter() {
  delete[] chars_;
  chars_ = NULL;
}

// Utility used to print plankton values as ascii.
class AsciiEncoder {
public:
  AsciiEncoder();

  // Writes the given value on the stream at the current indentation level.
  void write(variant_t value);

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
  void write_array(array_t array);

  // Writes a map value.
  void write_map(map_t map);

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
  static size_t get_short_length(variant_t variant, size_t offset);

  Buffer<char> chars_;
  size_t indent_;
  bool has_pending_newline_;
};

AsciiEncoder::AsciiEncoder()
  : indent_(0)
  , has_pending_newline_(false) { }

void AsciiEncoder::write(variant_t value) {
  switch (value.type()) {
    case variant_t::vtFalse:
      write_raw_string("%f");
      break;
    case variant_t::vtTrue:
      write_raw_string("%t");
      break;
    case variant_t::vtNull:
      write_raw_string("%n");
      break;
    case variant_t::vtInteger:
      write_integer(value.integer_value());
      break;
    case variant_t::vtString:
      write_string(value.string_chars(), value.string_length());
      break;
    case variant_t::vtBlob:
      write_blob(value.blob_data(), value.blob_size());
      break;
    case variant_t::vtArray:
      write_array(array_t(value));
      break;
    case variant_t::vtMap:
      write_map(map_t(value));
      break;
    default:
      write_raw_string("?");
      break;
  }
}

size_t AsciiEncoder::get_short_length(variant_t value, size_t offset) {
  switch (value.type()) {
    case variant_t::vtInteger:
      return offset + 5;
    case variant_t::vtFalse:
    case variant_t::vtTrue:
    case variant_t::vtNull:
      return offset + 2;
    case variant_t::vtString:
      return offset + value.string_length();
    case variant_t::vtArray: {
      array_t array = array_t(value);
      size_t current = offset + 2;
      for (size_t i = 0; i < array.length() && current < kShortLengthLimit; i++)
        current = get_short_length(array[i], current) + 2;
      return current;
    }
    case variant_t::vtMap: {
      map_t map = map_t(value);
      size_t current = offset + 2;
      variant_t key, value;
      for (map_t::iterator iter = map.iter();
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

void AsciiEncoder::write_raw_string(const char *chars) {
  flush_pending_newline();
  size_t size = strlen(chars);
  chars_.write(chars, size);
}

void AsciiEncoder::write_raw_char(char c) {
  flush_pending_newline();
  chars_.add(c);
}

void AsciiEncoder::flush_pending_newline() {
  if (!has_pending_newline_)
    return;
  chars_.add('\n');
  chars_.fill(' ', indent_);
  has_pending_newline_ = false;
}

void AsciiEncoder::schedule_newline() {
  has_pending_newline_ = true;
}

void AsciiEncoder::indent() {
  indent_ += 2;
}

void AsciiEncoder::deindent() {
  indent_ -= 2;
}

void AsciiEncoder::write_hex_byte(uint8_t c) {
  uint8_t high = (c >> 4) & 0xF;
  write_raw_char(high < 10 ? ('0' + high) : ('a' + high - 10));
  uint8_t low = c & 0xF;
  write_raw_char(low < 10 ? ('0' + low) : ('a' + low - 10));
}

void AsciiEncoder::write_integer(int64_t value) {
  char chars[64];
  sprintf(chars, "%li", value);
  write_raw_string(chars);
}

bool AsciiEncoder::is_unquoted_string_start(char c) {
  return ('a' <= c && c <= 'z')
      || ('A' <= c && c <= 'Z');
}

bool AsciiEncoder::is_unquoted_string_part(char c) {
  return is_unquoted_string_start(c)
      || ('0' <= c && c <= '9')
      || (c == '_');
}

bool AsciiEncoder::is_unescaped_char(char c) {
  if (c < ' ' || c > '~') {
    return false;
  } else {
    return (c != '"' && c != '\\');
  }
}

bool AsciiEncoder::is_unquoted(const char *chars, size_t length) {
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

void AsciiEncoder::write_string(const char *chars, size_t length) {
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

const char AsciiEncoder::kBase64Chars[65] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789"
    "+/";

const char AsciiEncoder::kBase64Padding = '=';

void AsciiEncoder::write_blob(const void *data, size_t size) {
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

void AsciiEncoder::write_array(array_t array) {
  bool is_long = get_short_length(array, indent_) >= kShortLengthLimit;
  write_raw_char('[');
  if (is_long) {
    indent();
    schedule_newline();
  }
  for (size_t i = 0; i < array.length(); i++) {
    variant_t value = array[i];
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

void AsciiEncoder::write_map(map_t map) {
  bool is_long = get_short_length(map, indent_) >= kShortLengthLimit;
  write_raw_char('{');
  if (is_long) {
    indent();
    schedule_newline();
  }
  variant_t key;
  variant_t value;
  for (map_t::iterator iter = map.iter(); iter.advance(&key, &value);) {
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

void AsciiEncoder::flush(TextWriter *writer) {
  flush_pending_newline();
  writer->length_ = chars_.length();
  chars_.add('\0');
  writer->chars_ = chars_.release();
}

void TextWriter::write(variant_t value) {
  AsciiEncoder encoder;
  encoder.write(value);
  encoder.flush(this);
}

// Utility for parsing a particular string.
class AsciiDecoder {
public:
  AsciiDecoder(const char *chars, size_t length, TextParser *parser);

  // Decode a toplevel variant expression. A toplevel expression is different
  // from others because it must fill the whole string.
  bool decode_toplevel(variant_t *out);

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
  bool fail(variant_t *out);

  // Succeeds parsing of some expression, returning true.
  bool succeed(variant_t value, variant_t *out);

  // Decodes a non-toplevel expression.
  bool decode(variant_t *out);

  // Parses the next integer.
  bool decode_integer(variant_t *out);

  // Parses the next unquoted string.
  bool decode_unquoted_string(variant_t *out);

  // Parses the next quoted string.
  bool decode_quoted_string(variant_t *out);

  // Parses the next binary blob.
  bool decode_blob(variant_t *out);

  // Parses the next array.
  bool decode_array(variant_t *out);

  // Parses the next map.
  bool decode_map(variant_t *out);

  // Returns the arena to use for allocation.
  arena_t *arena() { return parser_->arena_; }

  // Given a character, returns the special character it encodes (for instance
  // a newline for 'n'), or a null character if this one doesn't represent a
  // special character.
  static bool decode_short_escape(char c, char *out);

  size_t length_;
  size_t cursor_;
  const char *chars_;
  TextParser *parser_;
};

AsciiDecoder::AsciiDecoder(const char *chars, size_t length, TextParser *parser)
  : length_(length)
  , cursor_(0)
  , chars_(chars)
  , parser_(parser) {
  skip_whitespace();
}

void AsciiDecoder::skip_whitespace() {
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

bool AsciiDecoder::is_whitespace(char c) {
  switch (c) {
    case ' ': case '\n': case '\t': case '\f': case '\r':
      return true;
    default:
      return false;
  }
}

bool AsciiDecoder::is_newline(char c) {
  switch (c) {
    case '\n': case '\f':
      return true;
    default:
      return false;
  }
}

bool AsciiDecoder::is_digit(char c) {
  return '0' <= c && c <= '9';
}

bool AsciiDecoder::decode_toplevel(variant_t *out) {
  return decode(out) && (!has_more() || fail(out));
}

bool AsciiDecoder::decode(variant_t *out) {
  switch (current()) {
    case '%':
      advance();
      switch (current()) {
        case 'f':
          advance_and_skip();
          return succeed(variant_t::no(), out);
        case 't':
          advance_and_skip();
          return succeed(variant_t::yes(), out);
        case 'n':
          advance_and_skip();
          return succeed(variant_t::null(), out);
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
      } else if (AsciiEncoder::is_unquoted_string_start(c)) {
        return decode_unquoted_string(out);
      } else {
        return fail(out);
      }
  }
}

bool AsciiDecoder::decode_integer(variant_t *out) {
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
  return succeed(variant_t::integer(result), out);
}

bool AsciiDecoder::decode_unquoted_string(variant_t *out) {
  const char *start = chars_ + cursor_;
  while (AsciiEncoder::is_unquoted_string_part(current()))
    advance();
  const char *end = chars_ + cursor_;
  skip_whitespace();
  return succeed(arena()->new_string(start, end - start), out);
}

bool AsciiEncoder::encode_short_escape(char c, char *out) {
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

bool AsciiDecoder::decode_short_escape(char c, char *out) {
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

bool AsciiDecoder::decode_quoted_string(variant_t *out) {
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

bool AsciiDecoder::decode_array(variant_t *out) {
  advance_and_skip();
  array_t result = arena()->new_array();
  while (has_more() && current() != ']') {
    variant_t next;
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
  return succeed(result, out);
}

bool AsciiDecoder::decode_map(variant_t *out) {
  advance_and_skip();
  map_t result = arena()->new_map();
  while (has_more() && current() != '}') {
    variant_t key;
    if (!decode(&key))
      return fail(out);
    if (current() != ':')
      return fail(out);
    advance_and_skip();
    variant_t value;
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
  return succeed(result, out);
}

// An invalid sextet.
#define INV 255

// Padding marker.
#define PAD 254

const uint8_t AsciiDecoder::kBase64CharToSextet[256] = {
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

bool AsciiDecoder::decode_blob(variant_t *out) {
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
    variant_t result = arena()->new_blob(*data, data.length());
    return succeed(result, out);
  } else {
    return fail(out);
  }
}

bool AsciiDecoder::fail(variant_t *out) {
  parser_->has_failed_ = true;
  parser_->offender_ = current();
  *out = variant_t::null();
  return false;
}

bool AsciiDecoder::succeed(variant_t value, variant_t *out) {
  *out = value;
  return true;
}

TextParser::TextParser(arena_t *arena)
  : arena_(arena)
  , has_failed_(false)
  , offender_('\0') { }

variant_t TextParser::parse(const char *chars, size_t length) {
  has_failed_ = false;
  offender_ = '\0';
  AsciiDecoder decoder(chars, length, this);
  variant_t result;
  decoder.decode_toplevel(&result);
  return result;
}

} // namespace plankton
