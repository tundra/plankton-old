//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// C++ implementation of the plankton serialization format.

#ifndef _PLANKTON_HH
#define _PLANKTON_HH

#include "variant.hh"
#include "marshal.hh"

// Opaque C binding types.
struct pton_command_line_t { };
struct pton_command_line_reader_t { };
struct pton_syntax_error_t { };

namespace plankton {

// Utility for encoding plankton data. For most uses you can use a BinaryWriter
// to encode a whole variant at a time, but in cases where data is represented
// in some other way you can use this to build custom encoding.
class Assembler {
public:
  Assembler() : assm_(pton_new_assembler()) { }
  ~Assembler() { pton_dispose_assembler(assm_); }

  // Writes an array header for an array with the given number of elements. This
  // must be followed immediately by the elements.
  bool begin_array(uint32_t length) { return pton_assembler_begin_array(assm_, length); }

  // Writes a map header for a map with the given number of mappings. This must be
  // followed immediately by the mappings, keys and values alternating.
  bool begin_map(uint32_t size) { return pton_assembler_begin_map(assm_, size); }

  // Writes a seed header for a seed with the given number of headers and
  // fields. This must be followed immediately by the headers and body of the seed.
  bool begin_seed(uint32_t headerc, uint32_t fieldc) { return pton_assembler_begin_seed(assm_, headerc, fieldc); }

  // Writes the given boolean value.
  bool emit_bool(bool value) { return pton_assembler_emit_bool(assm_, value); }

  // Writes the null value.
  bool emit_null() { return pton_assembler_emit_null(assm_); }

  // Writes an int64 with the given value.
  bool emit_int64(int64_t value) { return pton_assembler_emit_int64(assm_, value); }

  // Writes a string with the default encoding.
  bool emit_default_string(const char *chars, uint32_t length) {
    return pton_assembler_emit_default_string(assm_, chars, length);
  }

  // Writes a blob with the given contents.
  bool emit_blob(const void *data, uint32_t size) { return pton_assembler_emit_blob(assm_, data, size); }

  // Writes the header for a string with a custom encoding.
  bool emit_string_with_encoding(pton_charset_t encoding, const void *chars, uint32_t length) {
    return pton_assembler_emit_string_with_encoding(assm_, encoding, chars, length);
  }

  // Writes an identity token.
  bool emit_id64(uint32_t size, uint64_t value) {
    return pton_assembler_emit_id64(assm_, size, value);
  }

  // Flushes the given assembler, writing the output into the given parameters.
  // The caller assumes ownership of the returned array and is responsible for
  // freeing it. This doesn't free the assembler, it must still be disposed with
  // pton_dispose_assembler.
  memory_block_t peek_code() { return pton_assembler_peek_code(assm_); }

private:
  pton_assembler_t *assm_;
};

// Utility for serializing variant values to plankton.
class BinaryWriter {
public:
  BinaryWriter();
  ~BinaryWriter();

  // Write the given value to this writer's internal buffer.
  void write(Variant value);

  // Returns the start of the buffer.
  uint8_t *operator*() { return bytes_; }

  // Returns the size in bytes of the data written to this writer's buffer.
  size_t size() { return size_; }

private:
  friend class VariantWriter;
  uint8_t *bytes_;
  size_t size_;
};

// The syntaxes text can be formatted as.
enum TextSyntax {
  SOURCE_SYNTAX,
  COMMAND_SYNTAX
};

// An object that holds the representation of a variant as a 7-bit ascii string.
class TextWriter {
public:
  TextWriter(TextSyntax syntax = SOURCE_SYNTAX);
  ~TextWriter();

  // Write the given variant to this asciigram.
  void write(Variant value);

  // After encoding, returns the string containing the encoded representation.
  const char *operator*() { return chars_; }

  // After encoding, returns the length of the string containing the encoded
  // representation.
  size_t length() { return length_; }

private:
  friend class TextWriterImpl;
  TextSyntax syntax_;
  char *chars_;
  size_t length_;
};

class AbstractTypeRegistry;

// Utility for reading variant values from serialized data.
class BinaryReader {
public:
  // Creates a new reader that allocates values from the given arena.
  BinaryReader(Factory *factory);

  // Deserializes the given input and returns the result as a variant.
  Variant parse(const void *data, size_t size);

  // Sets the type registry to use to resolve types during parsing.
  void set_type_registry(AbstractTypeRegistry *value) { type_registry_ = value; }

private:
  friend class BinaryReaderImpl;
  Factory *factory_;
  AbstractTypeRegistry *type_registry_;
};

// Represents a syntax error while parsing text input. If parsing fails an
// instance of this will be returned. You can then distinguish success/failure
// by checking whether you got a syntax error back or, more reliably in case
// you might have to deal with parsing correctly encoded syntax error objects,
// using TextReader::has_failed().
class SyntaxError : public pton_syntax_error_t {
public:
  SyntaxError(const char *source, size_t offset)
    : source_(source)
    , offset_(offset) { }

  // Returns the offending character.
  char offender() { return source_[offset_]; }

  // Returns the 0-based character offset within the source string where the
  // error occurred.
  size_t offset() { return offset_; }

  // The seed type for syntax errors.
  static SeedType<SyntaxError> *seed_type() { return &kSeedType; }

private:
  static SeedType<SyntaxError> kSeedType;
  const char *source_;
  size_t offset_;
};

// Utility for converting a plankton variant to a 7-bit ascii string.
class TextReader {
public:
  // Creates a new reader which uses the given arena for allocation.
  TextReader(TextSyntax syntax = SOURCE_SYNTAX, Factory *factory = NULL);
  ~TextReader();

  // Parse the given input, returning the value. If any errors occur a syntax
  // error will be returned; alternatively has_failed() and error() can be used
  // to inspect what went wrong.
  Variant parse(const char *chars, size_t length);

  // Returns true iff the last parse failed.  If parse hasn't been called at all
  // returns false.
  bool has_failed() { return error_ != NULL; }

  // If has_failed() returns true this will return the syntax error. The error
  // will have been allocated in the factory so will live as long as the factory
  // does.
  SyntaxError *error() { return error_; }

protected:
  friend class TextReaderImpl;
  Factory *factory_;
  Arena *scratch_arena_;
  TextSyntax syntax_;
  SyntaxError *error_;
};


// The result of parsing a set of command-line arguments.
class CommandLine : public pton_command_line_t {
public:
  // Constructor for failed results.
  explicit CommandLine(SyntaxError *error)
    : error_(error) { }

  // Constructor for successful results.
  CommandLine(Array args, Map options)
    : error_(NULL)
    , args_(args)
    , options_(options) { }

  // Returns the number of toplevel arguments.
  size_t argument_count() { return args_.length(); }

  // Returns the i'th argument. If the index is out of bounds returns null.
  Variant argument(size_t i) { return args_[i]; }

  // Returns the option with the given name. Returns the default value of there
  // is no such option, the default default (what?) being null.
  Variant option(Variant field, Variant defawlt = Variant::null());

  // The number of options passed.
  size_t option_count() { return options_.size(); }

  // Does this command line represent a successful parse?
  bool is_valid() { return error_ == NULL; }

  // If this command line is the result of a failed parse, returns the syntax
  // error that describes the problem. Otherwise NULL.
  SyntaxError *error() { return error_; }

  // The seed type for syntax errors.
  static SeedType<CommandLine> *seed_type() { return &kSeedType; }

private:
  static SeedType<CommandLine> kSeedType;
  SyntaxError *error_;
  Array args_;
  Map options_;
};

// A text reader specialized for reading command-line arguments. The plain text
// reader can also read the command-line value syntax, but this one has some
// more convenience methods and, particularly, reads the top-level command-line
// syntax which is slightly different from the value command-line syntax. So
// for instance, this
//
//   git checkout foo
//
// is not a valid single command-line value -- it is three strings -- but it is
// fine as a top-level command-line.
class CommandLineReader : public TextReader, public pton_command_line_reader_t {
public:
  // Creates a new reader which uses the given factory for allocation. If no
  // factory is specified one will be created and disposed when the reader is
  // destroyed.
  CommandLineReader(Factory *factory = NULL)
    : TextReader(COMMAND_SYNTAX, factory) { }

  // Parse the given input as a top-level command-line.
  CommandLine *parse(const char *chars, size_t length);

  // Parse the given program arguments as top-level command-line.
  CommandLine *parse(int argc, const char **argv);

  // Joins an argument array into a single string by inserting spaces between
  // the parts. Visible for testing only, don't use this.
  static char *join_argv(int argc, const char **argv, int *len_out);
};

} // namespace plankton

#endif // _PLANKTON_HH
