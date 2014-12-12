#!/usr/bin/python
# Copyright 2014 the Neutrino authors (see AUTHORS).
# Licensed under the Apache License, Version 2.0 (see LICENSE).


import plankton
import unittest
import StringIO


class ContainerTest(unittest.TestCase):

  def _read_socket(self, socket):
    stream = socket.get_root_stream()
    while socket.process_next_instruction():
      if not stream.is_empty():
        yield stream.take()

  def test_socket(self):
    outstr = StringIO.StringIO()
    out = plankton.OutputSocket(outstr)
    out.set_default_string_encoding("UTF-8")
    out.send_value([1, 2, 3])
    out.send_value({"a": 3})
    instr = plankton.InputSocket(StringIO.StringIO(outstr.getvalue()))
    self.assertTrue(instr.init())
    values = self._read_socket(instr)
    self.assertEquals([1, 2, 3], values.next())
    self.assertEquals({"a": 3}, values.next())


if __name__ == '__main__':
  runner = unittest.TextTestRunner(verbosity=0)
  unittest.main(testRunner=runner)
