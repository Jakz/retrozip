#pragma once

#include "memory_buffer.h"

class data_source
{
  
public:
  virtual bool eos() const = 0;

  virtual size_t read(void* dest, size_t size, size_t count) const = 0;
  template<typename T> size_t read(T& dest) const { return read(&dest, sizeof(T), 1); }
};

class memory_data_source : public data_source
{
private:
  memory_buffer _data;
  
public:
  memory_data_source() { }
  memory_data_source(byte* data, size_t length, bool copy = false) : _data(data, length, copy) { }
  memory_data_source(const byte* data, size_t length) : _data(data, length) { }

  memory_buffer& data() { return _data; }
  
  bool eos() const override { return _data.position() == _data.size(); }
  size_t length() const { return _data.size(); }
  size_t read(void* dest, size_t size, size_t count) const override { return _data.read(dest, size, count); }
};


class data_sink
{
  
public:
  virtual size_t write(const void* src, size_t size, size_t count) = 0;
};

class memory_data_sink : public data_sink
{
private:
  memory_buffer _data;
  
public:
  memory_data_sink() : _data() { }
  memory_buffer& data() { return _data; }
  
  size_t write(const void* src, size_t size, size_t count) override { return _data.write(src, size, count); }
};

struct data_buffer
{
private:
  byte* _buffer;
  size_t _offset;
  size_t _size;
  
public:
  data_buffer(size_t size) : _buffer(new byte[size]), _offset(0), _size(size) { }
  ~data_buffer() { delete [] _buffer; }
  
  bool empty() const { return _offset == 0; }
  bool full() const { return _offset == _size; }
  
  size_t size() const { return _size; }
  size_t available() const { return _size - _offset; }
  size_t used() const { return _offset; }
  
  void resize(size_t newSize)
  {
    if (newSize != _size)
    {
      _buffer = (byte*)realloc(_buffer, newSize);
      _size = newSize;
    }
  }
  
  void advance(size_t offset) { _offset += offset; }
  void consume(size_t amount)
  {
    if (_offset != amount)
      memmove(_buffer, _buffer + amount, _offset - amount);
    _offset -= amount;
  }
  
  byte* head() { return _buffer; }
  byte* tail() { return _buffer + _offset; }
  
  data_buffer& operator+=(const size_t offset) { this->advance(offset); return *this; }
};


class data_pipe
{
private:
  const data_source* _source;
  data_sink* _sink;
  
  data_buffer _buffer;

public:
  data_pipe(const data_source* source, data_sink* sink, size_t bufferSize) : _source(source), _sink(sink), _buffer(bufferSize)
  { }
  
  void stepInput()
  {
    /* available data to read is minimum between free room in buffer and remaining data */
    //size_t available = std::min(_bufferSize - _bufferPosition, _source->length() - _done);
    size_t available = _buffer.available();
    
    if (available > 0)
    {
      size_t effective = _source->read(_buffer.tail(), 1, available);
      _buffer.advance(effective);
    }
  }
  
  void stepOutput()
  {
    /* if there is data to process */
    if (!_buffer.empty())
    {
      size_t effective = _sink->write(_buffer.head(), 1, _buffer.used());
      
      /* TODO: circular buffer would be better? */
      /* we processed less data than total available, so we shift remaining */
      _buffer.consume(effective);
    }
  }
  
  void process()
  {
    while (!_source->eos())
    {
      stepInput();
      stepOutput();
    }
  }
};

class data_mutator
{
protected:
  const data_source* _source;
  data_sink* _sink;
  
  data_buffer _in, _out;

protected:
  virtual bool mutate() = 0;
  virtual void initialize() = 0;
  virtual void finalize() = 0;
  
public:
  data_mutator(const data_source* source, data_sink* sink, size_t inBufferSize, size_t outBufferSize) :
  _source(source), _sink(sink), _in(inBufferSize), _out(outBufferSize) { }

  void stepInput()
  {
    if (!_in.full())
    {
      size_t effective = _source->read(_in.tail(), 1, _in.available());
      _in.advance(effective);
    }
  }
  
  void stepOutput()
  {
    if (!_out.empty())
    {
      size_t effective = _sink->write(_out.head(), 1, _out.used());
      _out.consume(effective);
    }
  }
  
  void process()
  {
    initialize();
    
    while (!_source->eos())
    {
      stepInput();
      if (!mutate()) break;
      stepOutput();
    }
    
    while (mutate())
      stepOutput();
    
    finalize();
    
    while (!_out.empty())
      stepOutput();
  }
  
};





