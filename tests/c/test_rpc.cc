//- Copyright 2015 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "test/asserts.hh"
#include "test/unittest.hh"
#include "rpc.hh"

using namespace plankton;

TEST(rpc, message) {
  Message message;
  Arena arena;
  Native native = arena.new_native(&message);

}
