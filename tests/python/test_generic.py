#!/usr/bin/python
# Copyright 2014 the Neutrino authors (see AUTHORS).
# Licensed under the Apache License, Version 2.0 (see LICENSE).


import plankton
import collections
import os.path
import unittest
import yaml


class TestCase(object):

  def __init__(self, data):
    self.test_type = None
    self.entries = []
    for clause in data:
      test_type = clause.get("test_type", None)
      if not test_type is None:
        self.test_type = test_type
      else:
        self.entries.append(clause)

  def get_test_type(self):
    return self.test_type

  def __iter__(self):
    return iter(self.entries)


class GenericTest(unittest.TestCase):

  def get_test(self, test_type):
    tests_yaml = os.path.join(os.path.dirname(__file__), "..", "generic", "tests.yaml")
    self.assertTrue(os.path.exists(tests_yaml))
    raw_yaml = yaml.load(open(tests_yaml, "rt"))
    tests = self.adapt_yaml(raw_yaml)
    for block in tests:
      candidate = TestCase(block["test_case"])
      if candidate.get_test_type() == test_type:
        return candidate
    return None

  def adapt_yaml(self, value):
    if type(value) == list:
      return map(self.adapt_yaml, value)
    elif type(value) == dict:
      new_items = [(self.adapt_yaml(k), self.adapt_yaml(v)) for (k, v) in value.items()]
      adapted = collections.OrderedDict(new_items)
      if len(adapted) == 1 and ("object" in adapted):
        return self.adapt_object(adapted["object"])
      else:
        return adapted
    else:
      return value

  def adapt_object(self, map):
    type = None
    fields = collections.OrderedDict()
    for entry in map:
      if "type" in entry:
        type = entry["type"]
      else:
        [(f, v)] = entry.items()
        fields[f] = v
    return plankton.DefaultObject(type, fields)

  _PLANKTON_TO_PYTHON_TYPES = {
    "null": "NoneType",
    "array": "list",
    "map": "OrderedDict",
    "obj": "DefaultObject"
  }

  def test_datatypes(self):
    for test_case in self.get_test("datatypes"):
      found_type = type(test_case["value"]).__name__
      plankton_type = test_case['type']
      expected_type = GenericTest._PLANKTON_TO_PYTHON_TYPES.get(plankton_type, plankton_type)
      self.assertEquals(expected_type, found_type)


if __name__ == '__main__':
  runner = unittest.TextTestRunner(verbosity=0)
  unittest.main(testRunner=runner)
