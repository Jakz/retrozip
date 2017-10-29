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


class data_pipe
{
private:
  const data_source* _source;
  data_sink* _sink;
  
  byte* _buffer;
  
  size_t _bufferPosition;
  size_t _bufferSize;
  size_t _done;
  
public:
  data_pipe(const data_source* source, data_sink* sink, size_t bufferSize) : _source(source), _sink(sink), _buffer(new byte[bufferSize]),
  _bufferSize(bufferSize), _bufferPosition(0), _done(0) { }
  ~data_pipe() { delete [] _buffer; }
  
  void stepInput()
  {
    /* available data to read is minimum between free room in buffer and remaining data */
    //size_t available = std::min(_bufferSize - _bufferPosition, _source->length() - _done);
    size_t available = _bufferSize - _bufferPosition;

    if (available > 0)
    {
      size_t effective = _source->read(_buffer + _bufferPosition, 1, available);
      _bufferPosition += effective;
    }
  }
  
  void stepOutput()
  {
    /* if there is data to process */
    if (_bufferPosition > 0)
    {
      size_t effective = _sink->write(_buffer, 1, _bufferPosition);
      
      /* TODO: circular buffer would be better? */
      /* we processed less data than total available, so we shift remaining */
      if (effective != _bufferPosition)
        memmove(_buffer, _buffer + effective, _bufferPosition - effective);
      
      _bufferPosition = _bufferPosition - effective;
      _done += effective;
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
