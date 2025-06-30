#pragma once

#include "data_source.h"

#include <vector>

struct weak_data_source : public seekable_data_source
{
protected:
  const std::vector<uint8_t>& _data;
  size_t _position;

public:
  weak_data_source(const std::vector<uint8_t>& data) :
    _data(data), _position(0) { }

  size_t read(byte* dest, size_t amount) override
  {
    if (_position >= _data.size())
      return 0;

    size_t effective = std::min(amount, _data.size() - _position);
    std::copy(_data.begin() + _position, _data.begin() + _position + effective, dest);
    _position += effective;
    return effective;
  }
  
  void seek(roff_t position) override { _position = position; }
  size_t size() const override { return _data.size(); }
  roff_t tell() const override { return _position; }
};

struct weak_vector_sink : public data_sink
{
protected:
  std::vector<uint8_t>& _data;
  size_t _position;
  bool _autogrow;

public:
  weak_vector_sink(std::vector<uint8_t>& data) : 
    _data(data), _position(0), _autogrow(true) { }

  size_t write(const byte* src, size_t amount) override
  {
   if (_autogrow)
      _data.resize(std::max(_data.size(), _position + amount));
   else
   {
     if (_position >= _data.size())
       return 0;
   }
    
    size_t effective = std::min(amount, _data.size() - _position);
    std::copy(src, src + effective, _data.begin() + _position);
    _position += effective;
    return effective;
  }
};
