//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "test/asserts.hh"
#include "test/unittest.hh"
#include "plankton-binary.hh"

#include <fstream>
#include <yaml-cpp/yaml.h>

using namespace plankton;

// A test case read from a yaml test spec.
class YamlTestCase {
public:
  YamlTestCase() { }
  YamlTestCase(const YAML::Node &node);

  // Returns the type of this test case.
  std::string test_type() { return test_type_; }

  // Returns the number of test clauses in this case.
  size_t size() { return clauses_.size(); }

  // Returns the i'th test clause.
  const YAML::Node &operator[](size_t i) { return node_->operator[](clauses_[i]); }

  // Reads and returns the test case of the given type from the test spec.
  // Returns true if a spec was successfully read.
  static bool read_test_case(std::string type, YamlTestCase *test_case_out);

private:
  // Clone of the test_case node this test case was created from.
  std::auto_ptr<YAML::Node> node_;

  // Cache of the test type.
  std::string test_type_;

  // Mapping from test clause index to elements in the node. This is how we skip
  // the elements that represent metadata, like the test type.
  std::vector<int> clauses_;
};

YamlTestCase::YamlTestCase(const YAML::Node &node) {
  const YAML::Node &test_case = node["test_case"];
  node_ = test_case.Clone();
  for (size_t i = 0; i < test_case.size(); i++) {
    const YAML::Node &clause = test_case[i];
    if (const YAML::Node *test_type = clause.FindValue("test_type")) {
      // Skip this clause if this is the test_type entry.
      *test_type >> test_type_;
    } else {
      // Otherwise add it to the list of clauses.
      clauses_.push_back(i);
    }
  }
}

bool YamlTestCase::read_test_case(std::string type, YamlTestCase *test_case_out) {
  std::ifstream fin("/home/plesner/Documents/plankwork/plankton/tests/generic/tests.yaml");
  YAML::Parser parser(fin);
  YAML::Node root;
  ASSERT_TRUE(parser.GetNextDocument(root));
  for (size_t i = 0; i < root.size(); i++) {
    YamlTestCase test_case(root[i]);
    if (test_case.test_type() == type) {
      *test_case_out = test_case;
      return true;
    }
  }
  return false;
}

TEST(generic, datatypes) {
  YamlTestCase test_case;
  ASSERT_TRUE(YamlTestCase::read_test_case("datatypes", &test_case));
  for (size_t i = 0; i < test_case.size(); i++) {
    const YAML::Node &clause = test_case[i];
    std::string type;
    clause["type"] >> type;
    std::cout << type << std::endl;
  }
}
