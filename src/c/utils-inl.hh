//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// Various utilities for working with plankton.

#ifndef _PLANKTON_UTILS_INL
#define _PLANKTON_UTILS_INL

#include "plankton.hh"

namespace plankton {

template <typename T>
class Buffer {
public:
  Buffer();
  ~Buffer();

  void add(const T &value);

  void write(const T *data, size_t count);

  void fill(const T &value, size_t count);

  T *operator*() { return data_; }

  size_t length() { return cursor_; }

  // Returns the contents of this buffer and then removes any references to it
  // such that it will not be disposed when this buffer is destroyed.
  T *release();

private:
  void ensure_capacity(size_t size);

  size_t capacity_;
  size_t cursor_;
  T *data_;
};

template <typename T>
Buffer<T>::Buffer()
  : capacity_(0)
  , cursor_(0)
  , data_(NULL) { }

template <typename T>
Buffer<T>::~Buffer() {
  delete[] data_;
  data_ = NULL;
}

template <typename T>
void Buffer<T>::add(const T &value) {
  ensure_capacity(1);
  data_[cursor_++] = value;
}

template <typename T>
void Buffer<T>::fill(const T &value, size_t count) {
  ensure_capacity(count);
  for (size_t i = 0; i < count; i++)
    data_[cursor_++] = value;
}

template <typename T>
void Buffer<T>::write(const T *data, size_t count) {
  ensure_capacity(count);
  memcpy(data_ + cursor_, data, count * sizeof(T));
  cursor_ += count;
}

template <typename T>
void Buffer<T>::ensure_capacity(size_t size) {
  size_t required = cursor_ + size;
  if (required < capacity_)
    return;
  size_t new_capacity = (required < 128) ? 256 : (2 * required);
  T *new_data = new T[new_capacity];
  if (cursor_ > 0)
    memcpy(new_data, data_, sizeof(T) * cursor_);
  delete[] data_;
  data_ = new_data;
  capacity_ = new_capacity;
}

template <typename T>
T *Buffer<T>::release() {
  T *result = data_;
  data_ = NULL;
  return result;
}

} // namespace plankton

#endif // _PLANKTON_UTILS_INL
