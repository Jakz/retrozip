#pragma once

#include "base/common.h"
#include "data_source.h"

enum class Seek
{
  SET = SEEK_SET,
  END = SEEK_END,
  CUR = SEEK_CUR
};

template<typename T> class data_reference;
template<typename T> class array_reference;

class memory_buffer : public seekable_data_source, public data_sink
{
private:
  byte* _data;

  mutable off_t _position;
  size_t _capacity;
  size_t _size;
  
  bool _dataOwned;
  
public:
  memory_buffer() : memory_buffer(KB16)
  {
    static_assert(sizeof(off_t) == 8, "");
    static_assert(sizeof(size_t) == 8, "");
  }
  
  memory_buffer(const byte* data, size_t length) : _capacity(length), _size(length), _position(0), _dataOwned(true)
  {
    _data = new byte[length];
    std::copy(data, data + length, _data);
  }

  
  memory_buffer(byte* data, size_t length, bool copy) : _data(data), _capacity(length), _size(length), _position(0), _dataOwned(copy)
  {
    if (copy)
    {
      _data = new byte[length];
      std::copy(data, data + length, _data);
    }
  }
  
  memory_buffer(memory_buffer&& other) : _data(other._data), _position(other._position), _size(other._size), _capacity(other._capacity), _dataOwned(other._dataOwned)
  {
    if (_dataOwned)
    {
      other._data = nullptr;
      other._position = 0;
      other._capacity = 0;
      other._size = 0;
      other._dataOwned = false;
    }
  }
  
  memory_buffer& operator=(memory_buffer&& other)
  {
    if (_dataOwned)
      delete [] _data;
    
    _data = other._data;
    _position = other._position;
    _size = other._size;
    _capacity = other._capacity;
    _dataOwned = other._dataOwned;
    
    return *this;
  }
  
  memory_buffer(memory_buffer&) = delete;
  memory_buffer& operator=(memory_buffer&) = delete;

  
  memory_buffer(size_t capacity) : _data(new byte[capacity]), _capacity(capacity), _size(0), _position(0), _dataOwned(true) { }
  ~memory_buffer() { if (_dataOwned) delete [] _data; }
  
  byte operator[](size_t index) const { return _data[index]; }
  
  bool operator==(const memory_buffer& other) const { return _size == other._size && std::equal(_data, _data+_size, other._data); }
  bool operator!=(const memory_buffer& other) { return !operator==(other); }
  
  size_t size() const override { return _size; }
  size_t capacity() const { return _capacity; }
  off_t position() const { return _position; }
  
  const byte* raw() const { return _data; }
  byte* raw() { return _data; }
  
  const byte* data() const { return _data; }
  byte* data() { return _data; }
  
  off_t tell() const override { return _position; }
  
  void seek(off_t offset) override { seek(offset, Seek::SET); }
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
      TRACE_MB("%p: memory_buffer::ensure_capacity (old: %lu, new: %lu)", this, _capacity, capacity);
      //TODO: this may fail and must be managed
      byte* newData = new byte[capacity];
      memset(newData, 0, capacity);
      std::copy(_data, _data+_size, newData);
      delete[] _data;
      this->_data = newData;
      this->_capacity = capacity;
    }
  }

  template<typename T> data_reference<T> reserve();
  template<typename T> array_reference<T> reserveArray(size_t size);
  
  void reserve(size_t size)
  {
    ensure_capacity(_position + size);
    _position += size;
  }
  
  template<typename T> size_t write(const T& src) { return write(&src, sizeof(T), 1); }
  size_t write(const void* data, size_t size, size_t count)
  {
    size_t requiredCapacity = _position + size*count;
    
    if (requiredCapacity > _capacity)
    {
      size_t delta = std::max(_capacity/2, requiredCapacity - _capacity);
      ensure_capacity(_capacity + delta);
    }

    std::copy((const byte*)data, (const byte*)data + (size*count), _data+_position);
    _position += count*size;
    _size = std::max(_size, (size_t)_position);
    return count*size;
  }
  
  template<typename T> size_t read(T& dest) { return read(&dest, sizeof(T), 1); }
  size_t read(void* data, size_t size, size_t count)
  {
    size_t available = std::min(_size - _position, (off_t)size*count);
    std::copy(_data + _position, _data + _position + available, (byte*)data);
    _position += available;
    return available;
  }
  
  memory_buffer& trim()
  {
    if (_capacity > _size)
    {
      byte* data = new byte[_size];
      std::copy(_data, _data + _size, data);
      delete [] _data;
      _data = data;
      _capacity = _size;
    }
    
    return *this;
  }
  
  bool serialize(const file_handle& file) const
  {
    return file.write(_data, 1, _size);
  }
  
  void unserialize(const file_handle& file)
  {
    size_t length = file.length();
    reserve(length);
    _size = length;
    rewind();
    file.read(_data, 1, _size);
  }
  
  /* orthogonal interface meant to let client data write directly to data buffer 
     through pointer without random access
   */
  bool empty() const { return _size == 0; }
  bool full() const { return _size == _capacity; }
  size_t available() const { return _capacity - _size; }
  size_t used() const { return _size; }
  
  size_t read(byte* data, size_t amount) override
  {
    if (_size == _position)
    {
      TRACE_MB("%p: memory_buffer::read EOS", this);
      return END_OF_STREAM;
    }
    
    //TRACE("buffer read %lu (position: %lu/%lu)", amount, _size, _position);
    TRACE_MB("%p: memory_buffer::read %lu (size: %lu/%lu)", this, amount, _position, _capacity);
    return read(data, 1, amount);
  }
  
  size_t write(const byte* data, size_t amount) override
  {
    if (amount != END_OF_STREAM)
    {
      TRACE_MB("%p: memory_buffer::write %lu (size: %lu/%lu)", this, amount, amount+_size, _capacity);
      return write(data, 1, amount);
    }
    else
    {
      TRACE_MB("%p: memory_buffer::read write EOS -> EOS", this);
      return END_OF_STREAM;
    }
  }
  
  void resize(size_t newCapacity)
  {
    if (newCapacity > _capacity)
    {
      assert(_dataOwned);
      //TODO: this may fail and must be managed
      _data = (byte*)realloc(_data, newCapacity);
      _capacity = newCapacity;
    }
  }
  
  void advance(size_t offset)
  {
    _size += offset;
    TRACE_MB("%p: memory_buffer::advance %lu (%lu/%lu)", this, offset, _size, _capacity);
  }
  
  void consume(size_t amount)
  {
    if (_size != amount)
      memmove(_data, _data + amount, _size - amount);
    _size -= amount;
    TRACE_MB("%p: memory_buffer::consume %lu (%lu/%lu)", this, amount, _size, _capacity);
  }
  
  byte* head() { return _data; }
  byte* tail() { return _data + _size; }
};

template<typename T>
class data_reference
{
private:
  memory_buffer* _buffer;
  off_t _position;
  data_reference(memory_buffer& buffer, off_t position) : _buffer(&buffer), _position(position) { }
  
public:
  data_reference() : _buffer(nullptr), _position(0) { }
  operator off_t() const { return _position; }

  void write(const T& value)
  {
    assert(_buffer);
    off_t mark = _buffer->tell();
    _buffer->seek(_position, Seek::SET);
    _buffer->write(&value, sizeof(T), 1);
    _buffer->seek(mark, Seek::SET);
  }

  friend class memory_buffer;
};

template<typename T>
class array_reference
{
private:
  memory_buffer* _buffer;
  off_t _position;
  size_t size;
  array_reference(memory_buffer& buffer, off_t position, size_t size) : _buffer(&buffer), _position(position) { }
  
public:
  array_reference() : _buffer(nullptr), _position(0) { }
  operator off_t() const { return _position; }
  
  void write(const T& value, size_t index)
  {
    assert(_buffer);
    off_t mark = _buffer->tell();
    _buffer->seek(_position + sizeof(T)*index, Seek::SET);
    _buffer->write(&value, sizeof(T), 1);
    _buffer->seek(mark, Seek::SET);
  }
  
  void read(T& value, size_t index)
  {
    assert(_buffer);
    off_t mark = _buffer->tell();
    _buffer->seek(_position + sizeof(T)*index, Seek::SET);
    _buffer->read(value);
    _buffer->seek(mark, Seek::SET);
  }
  
  friend class memory_buffer;
};

template<typename T> data_reference<T> memory_buffer::reserve()
{
  off_t mark = tell();
  assert(mark == _size);
  reserve(sizeof(T));
  _size += sizeof(T);
  return data_reference<T>(*this, mark);
}

template<typename T> array_reference<T> memory_buffer::reserveArray(size_t size)
{
  off_t mark = tell();
  assert(mark == _size);
  reserve(sizeof(T)*size);
  _size += sizeof(T)*size;
  return array_reference<T>(*this, mark, size);
}
