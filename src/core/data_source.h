#pragma once

#include "base/common.h"

constexpr size_t END_OF_STREAM = 0xFFFFFFFFFFFFFFFFLL;

class data_source
{
public:
  virtual size_t read(byte* dest, size_t amount) = 0;
  template<typename T> size_t read(T& dest) const { return read(&dest, sizeof(T), 1); }
};

class multiple_data_source : public data_source
{
private:
  using iterator = std::vector<data_source*>::const_iterator;
  
  bool _pristine;
  std::function<void(data_source*)> _onBegin;
  std::function<void(data_source*)> _onEnd;
  
  std::vector<data_source*> _sources;
  iterator _it;
  
public:
  multiple_data_source(const std::vector<data_source*>& sources) :
  _pristine(true), _sources(sources), _it(_sources.begin()),
  _onBegin([](data_source*){}), _onEnd([](data_source*){}) {}
  
  void setOnBegin(std::function<void(data_source*)> onBegin) { this->_onBegin = onBegin; }
  void setOnEnd(std::function<void(data_source*)> onEnd) { this->_onEnd = onEnd; }
  
  size_t read(byte* dest, size_t amount) override
  {
    if (_it == _sources.end())
      return END_OF_STREAM;
    else if (_pristine)
    {
      _onBegin(*_it);
      _pristine = false;
    }
    
    size_t effective = END_OF_STREAM;
    while (effective == END_OF_STREAM && _it != _sources.end())
    {
      effective = (*_it)->read(dest, amount);
      
      if (effective == END_OF_STREAM)
      {
        _onEnd(*_it);
        ++_it;
        _pristine = true;
      }
    }
    
    return effective;
  }
};

class data_sink
{
  
public:
  virtual size_t write(const byte* src, size_t amount) = 0;
  //virtual void eos() = 0;
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


class log_data_source : public data_source
{
private:
  size_t _length;
  size_t _position;
  size_t _maxAvailable;
  mutable bool _isEos;
  
public:
  log_data_source(size_t length, size_t maxAvailable) : _length(length), _position(0), _maxAvailable(maxAvailable), _isEos(false) { }
  
  size_t read(byte* dest, size_t amount) override
  {
    if (_position == _length)
      return END_OF_STREAM;
    
    size_t available = std::min(_maxAvailable, std::min(amount, _length - _position));
    printf("data_source::read(%lu) (%lu)\n", amount, available);
    _position += available;
    return available;
  }
};

class log_data_sink : public data_sink
{
private:
  size_t _maxAvailable;

public:
  log_data_sink(size_t maxAvailable) : _maxAvailable(maxAvailable) { }
  
  size_t write(const byte* src, size_t amount) override
  {
    if (amount != END_OF_STREAM)
    {
      size_t available = std::min(amount, _maxAvailable);
      printf("data_sink::write(%lu) (%lu)\n", amount, available);
      return available;
    }
    else
    {
      printf("data_sink::eos()\n");
      return END_OF_STREAM;
    }
  }
};
