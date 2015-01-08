//- Copyright 2015 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "stdc.h"

#include "marshal-inl.hh"
#include "rpc.hh"

using namespace plankton;

ObjectType<Message> Message::kType("rpc.Message");
