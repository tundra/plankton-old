//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

///

#ifndef _RPC_HH
#define _RPC_HH

#include "marshal.hh"
#include "plankton-inl.hh"

namespace plankton {

class Message {
public:
  static ObjectType<Message> *object_type() { return &kType; }
public:
  static ObjectType<Message> kType;
};

} // namespace plankton

#endif // _RPC_HH
