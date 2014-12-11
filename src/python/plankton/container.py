# Copyright 2013 the Neutrino authors (see AUTHORS).
# Licensed under the Apache License, Version 2.0 (see LICENSE).

# Utilities for encoding and decoding of plankton data.


from . import codec
import Queue


_SET_DEFAULT_STRING_ENCODING = 1
_SEND_VALUE = 2


class DefaultStream(object):

  def __init__(self):
    self.values = Queue.Queue()

  def receive_value(self, value):
    self.values.put(value)

  def is_empty(self):
    return self.values.empty()

  def take(self):
    return self.values.get_nowait()


# A raw value output stream.
class OutputSocket(object):

  # Creates a new output stream that writes to the given file (-like thing).
  def __init__(self, file):
    self.file = file
    self.cursor = 0
    self._write_header()

  # Writes the given ascii string value as the default string encoding.
  def set_default_string_encoding(self, value):
    self._write_byte(_SET_DEFAULT_STRING_ENCODING)
    self._write_value(bytearray(value.encode("ascii")))
    self._write_padding()

  def send_value(self, value):
    self._write_byte(_SEND_VALUE)
    self._write_value(value)
    self._write_padding()

  # Writes the given value to this stream.
  def _write_value(self, value):
    data = codec.Encoder().encode(value)
    assm = codec.EncodingAssembler()
    assm.uint32(len(data))
    self._write_blob(assm.bytes)
    self._write_blob(data)

  def _write_header(self):
    self.file.write("pt\xf6n\00\00\00\00")

  def _write_byte(self, byte):
    self.file.write(chr(byte))
    self.cursor += 1

  def _write_blob(self, bytes):
    self.file.write(bytes)
    self.cursor += len(bytes)

  def _write_padding(self):
    while (self.cursor % 8) != 0:
      self._write_byte(0)


class InputSocket(object):

  def __init__(self, file):
    self.file = file
    self.default_encoding = None
    self.cursor = 0
    self.root_stream = DefaultStream()

  def init(self):
    header = self._read_blob(8)
    if header == "pt\xf6n\00\00\00\00":
      return True
    else:
      self.file = None
      return False

  def process_next_instruction(self):
    opcode = self._get_byte()
    if opcode == _SET_DEFAULT_STRING_ENCODING:
      value = self._read_value()
      self.default_encoding = str(value)
      self._read_padding()
      return True
    elif opcode == _SEND_VALUE:
      value = self._read_value()
      self._read_padding()
      self.root_stream.receive_value(value)
      return True
    else:
      return False

  def get_root_stream(self):
    return self.root_stream

  def _read_blob(self, count):
    self.cursor += count
    return self.file.read(count)

  def _get_byte(self):
    next = self.file.read(1)
    if len(next) == 0:
      return None
    else:
      self.cursor += 1
      return ord(next)

  def _read_padding(self):
    while (self.cursor % 8) != 0:
      self._get_byte()

  def _read_value(self):
    size = codec.DataInputStream.decode_uint32_from(self)
    bytes = self._read_blob(size)
    return codec.DataInputStream(bytearray(bytes), None, None).read_object()
