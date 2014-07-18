//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// Various utilities for working with plankton.

#ifndef _PLANKTON_UTILS_INL
#define _PLANKTON_UTILS_INL

#include "plankton.hh"

namespace plankton {

// A simple buffer.
template <typename T>
class Buffer {
public:
  Buffer();
  ~Buffer();

  // Add the given value at the end of this buffer, expanding if necessary.
  void add(const T &value);

  // Add 'count' elements from the given array to the end of this buffer,
  // expanding if necessary.
  void write(const T *data, size_t count);

  // Add the given value 'count' times to the end of this buffer, expanding if
  // necessary.
  void fill(const T &value, size_t count);

  // Returns the start of this buffer. The buffer is only valid until the next
  // modification to the buffer.
  T *operator*() { return data_; }

  // Returns the number of elements in this buffer.
  size_t length() { return cursor_; }

  // Returns the contents of this buffer and then removes any references to it
  // such that it will not be disposed when this buffer is destroyed. The
  // buffer can not be changed after this has been called (though length()
  // remains valid).
  T *release();

private:
  // Ensures that this buffer will hold at least 'size' additional elements.
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
