//- Copyright 2013 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "c/stdc.h"

BEGIN_C_INCLUDES
#include "utils/crash.h"
#include "utils/log.h"
#include "utils/strbuf.h"
#include "utils/string-inl.h"
END_C_INCLUDES

#include "io/file.hh"
#include "plankton-binary.hh"

using namespace plankton;
using namespace tclib;

void disass_instr(pton_instr_t *instr, string_buffer_t *buf) {
  switch (instr->opcode) {
    case PTON_OPCODE_INT64:
      string_buffer_printf(buf, "int:%i", instr->payload.int64_value);
      break;
    case PTON_OPCODE_DEFAULT_STRING: {
      string_buffer_printf(buf, "default_string:%i \"", instr->payload.default_string_data.length);
      utf8_t str = new_string(
          reinterpret_cast<const char*>(instr->payload.default_string_data.contents),
          instr->payload.default_string_data.length);
      string_buffer_append(buf, str);
      string_buffer_printf(buf, "\"");
      break;
    }
    case PTON_OPCODE_BEGIN_ARRAY:
      string_buffer_printf(buf, "begin_array:%i", instr->payload.array_length);
      break;
    case PTON_OPCODE_BEGIN_MAP:
      string_buffer_printf(buf, "begin_map:%i", instr->payload.map_size);
      break;
    case PTON_OPCODE_BEGIN_SEED:
      string_buffer_printf(buf, "begin_seed:%i:%i", instr->payload.seed_data.headerc,
          instr->payload.seed_data.fieldc);
      break;
    case PTON_OPCODE_NULL:
      string_buffer_printf(buf, "null");
      break;
    case PTON_OPCODE_BOOL:
      string_buffer_printf(buf, instr->payload.bool_value ? "true" : "false");
      break;
    case PTON_OPCODE_REFERENCE:
      string_buffer_printf(buf, "get_ref:%i", instr->payload.reference_offset);
      break;
    default:
      string_buffer_printf(buf, "unknown (%i)", instr->opcode);
      break;
  }
}

int main(int argc, char *argv[]) {
  install_crash_handler();
  FileSystem *fs = FileSystem::native();
  OutStream *out = fs->std_out();
  for (int i = 1; i < argc; i++) {
    utf8_t name = new_c_string(argv[i]);
    InStream *in = NULL;
    if (string_equals_cstr(name, "-"))
      in = fs->std_in();
    else
      in = fs->open(name, OPEN_FILE_MODE_READ).in();
    string_buffer_t buf;
    string_buffer_init(&buf);
    do {
      char block[256];
      size_t count = in->read_bytes(block, 256);
      string_buffer_append(&buf, new_string(block, count));
    } while (!in->at_eof());
    utf8_t data = string_buffer_flush(&buf);
    if (false) {
    const uint8_t *code = reinterpret_cast<const uint8_t*>(data.chars);
    size_t offset = 0;
    while (offset < data.size) {
      pton_instr_t instr;
      if (!pton_decode_next_instruction(code, data.size - offset, &instr))
        break;
      string_buffer_t linebuf;
      string_buffer_init(&linebuf);
      string_buffer_printf(&linebuf, "%i: ", offset);
      disass_instr(&instr, &linebuf);
      utf8_t line = string_buffer_flush(&linebuf);
      out->printf("%s\n", line.chars);
      code += instr.size;
      offset += instr.size;
    }
    }
    Arena arena;
    BinaryReader reader(&arena);
    Variant result = reader.parse(data.chars, data.size);
    TextWriter writer;
    writer.write(result);
    out->printf("%s\n", *writer);
  }
}

