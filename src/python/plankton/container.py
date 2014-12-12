# Copyright 2013 the Neutrino authors (see AUTHORS).
# Licensed under the Apache License, Version 2.0 (see LICENSE).

# Utilities for encoding and decoding of plankton data.


from . import codec
import Queue
from abc import ABCMeta, abstractmethod
import codecs


_SET_DEFAULT_STRING_ENCODING = 1
_SEND_VALUE = 2


# Abstract implementation of a stream.
class AbstractStream(object):
  __metaclass__ = ABCMeta

  # Called by the socket when a new block of data is available to be processed
  # by this stream. The default implementation parses the data and passes the
  # result to receive_value.
  def receive_block(self, block):
    value = codec.DataInputStream(block, None, None).read_object()
    self.receive_value(value)

  # Custom handling of a new value being passed to this stream.
  @abstractmethod
  def receive_value(self, value):
    pass


class DefaultStream(AbstractStream):

  def __init__(self):
    self.values = Queue.Queue()

  def receive_value(self, value):
    self.values.put(value)

  def is_empty(self):
    return self.values.empty()

  def take(self):
    return self.values.get_nowait()


# A socket that knows how to write values and metadata to a file-like object.
class OutputSocket(object):

  # Creates a new output stream that writes to the given file (-like object).
  def __init__(self, file):
    self.file = file
    self.cursor = 0
    self.default_string_encoding = None
    self._write_header()

  # Writes the given ascii string value as the default string encoding.
  def set_default_string_encoding(self, value):
    self.default_string_encoding = value
    self._write_byte(_SET_DEFAULT_STRING_ENCODING)
    self._write_value(bytearray(codecs.encode(value, "ascii")))
    self._write_padding()

  # Sends the given value to the stream with the given id. If no id is given
  # the value is sent to the root stream.
  def send_value(self, value, stream_id=None):
    self._write_byte(_SEND_VALUE)
    self._write_value(stream_id)
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


# A socket that reads plankton from a file-like object.
class InputSocket(object):

  def __init__(self, file):
    self.file = file
    self.default_encoding = None
    self.cursor = 0
    self.streams = {}
    self.stream_factory = DefaultStream

  # Sets the function that will be called to produce new stream objects. Streams
  # don't have to extend AbstractStream but they do have to implement the same
  # interface.
  def set_stream_factory(self, factory):
    self.stream_factory = factory
    return self

  # Read the stream header and, if it is valid, initialize this socket
  # appropriately. Returns True if initialization succeeds.
  def init(self):
    header = self._read_blob(8)
    if header == "pt\xf6n\00\00\00\00":
      root_stream = (self.stream_factory)()
      self._register_stream(None, root_stream)
      return True
    else:
      self.file = None
      return False

  # Reads and processes the next instruction from the file. This will either
  # cause the internal state of the socket to be updated or a value to be
  # delivered to a stream. Returns True if an instruction was processed, False
  # if input was in valid or if there is no more input to fecth.
  def process_next_instruction(self):
    opcode = self._get_byte()
    if opcode == _SET_DEFAULT_STRING_ENCODING:
      value = self._read_value()
      self.default_encoding = str(value)
      self._read_padding()
      return True
    elif opcode == _SEND_VALUE:
      stream_id = self._read_value()
      block = self._read_block()
      self._read_padding()
      stream = self.streams.get(stream_id, None)
      if not stream is None:
        stream.receive_block(block)
      return True
    else:
      return False

  # Returns the root stream, the stream that receives messages sent to null.
  def get_root_stream(self):
    return self.streams.get(None, None)

  def _register_stream(self, id, stream):
    self.streams[id] = stream

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

  def _read_block(self):
    size = codec.DataInputStream.decode_uint32_from(self)
    return bytearray(self._read_blob(size))

  def _read_value(self):
    block = self._read_block()
    return codec.DataInputStream(block, None, None).read_object()
