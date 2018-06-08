#pragma once

#include "header.h"

#include "streams/memory_buffer.h"

class filter_options
{
protected:
  virtual void unserialize(byte* data, size_t length) = 0;
  virtual memory_buffer serialize() = 0;
  
public:
  
};

namespace options
{
  class xor_opt : public filter_options
  {
  private:
    std::vector<byte> key;
  };
}
