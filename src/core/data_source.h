#pragma once

#include "base/common.h"

class data_source
{
public:
  virtual bool eos() const = 0;

  virtual size_t read(void* dest, size_t size, size_t count) const = 0;
  template<typename T> size_t read(T& dest) const { return read(&dest, sizeof(T), 1); }
};

class data_sink
{
  
public:
  virtual size_t write(const void* src, size_t size, size_t count) = 0;
};

struct data_buffer
{
  virtual bool empty() const = 0;
  virtual bool full() const = 0;
  
  virtual size_t size() const= 0;
  virtual size_t available() const = 0;
  virtual size_t used() const = 0;
  
  virtual void resize(size_t newSize) = 0;
  
  virtual void advance(size_t offset) = 0;
  virtual void consume(size_t amount) = 0;
  
  virtual byte* head() = 0;
  virtual byte* tail() = 0;
};
