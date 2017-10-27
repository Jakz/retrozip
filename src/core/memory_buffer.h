#pragma once

#include "archive.h"
#include "base/common.h"

class memory_buffer
{
private:
  byte* _data;

  off_t _position;
  size_t _capacity;
  size_t _size;
  
public:
  memory_buffer() : memory_buffer(16384)
  {
    static_assert(sizeof(off_t) == 8, "");
    static_assert(sizeof(size_t) == 8, "");
  }
  
  memory_buffer(size_t capacity) : _data(new byte[capacity]), _capacity(capacity), _size(0), _position(0) { }
  ~memory_buffer() { delete [] _data; }
  
  size_t size() const { return _size; }
  size_t capacity() const { return _capacity; }
  off_t position() const { return _position; }
  const byte* raw() { return _data; }
  
  off_t tell() const { return _position; }
  
  void seek(off_t offset, Seek origin)
  {
    switch (origin) {
      case Seek::CUR: _position += offset; _position = std::max(0LL, _position); break;
      case Seek::SET: _position = std::max(0LL, offset); break;
      case Seek::END: _position = _size + offset; _position = std::max(0LL, _position); break;
    }
  }
  
  void rewind() { seek(0, Seek::SET); }
  
  void ensure_capacity(size_t capacity)
  {
    if (capacity > _capacity)
    {
      byte* newData = new byte[capacity];
      memset(newData, 0, capacity);
      std::copy(_data, _data+_size, newData);
      delete[] _data;
      this->_data = newData;
      this->_capacity = capacity;
    }
  }
  
  /*void reserve(size_t size)
  {
    assert(_position == _size);
    ensure
    
    if (size > _capacity - _size)
    {
      size_t newCapacity = size + _size;
      byte* newData = new byte[newCapacity];
      memset(newData, 0, newCapacity);
      std::copy(_data, _data+_size, newData);
      delete[] _data;
      this->_data = newData;
      this->_capacity = newCapacity;
    }
  }*/
  
  void reserve(size_t size)
  {
    ensure_capacity(_position + size);
    _position += size;
  }
  
  void write(const void* data, size_t size, size_t count)
  {
    ensure_capacity(_position + (size*count));

    std::copy((const byte*)data, (const byte*)data + (size*count), _data+_position);
    _position += count*size;
    _size = std::max(_size, (size_t)_position);
  }
  
  void trim()
  {
    if (_capacity > _size)
    {
      byte* data = new byte[_size];
      std::copy(_data, _data + _size, data);
      delete [] _data;
      _data = data;
      _capacity = _size;
    }
  }
};
