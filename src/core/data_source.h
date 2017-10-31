#pragma once

#include "base/common.h"

class data_source
{
public:
  virtual bool eos() const = 0;

  virtual size_t read(void* dest, size_t amount) = 0;
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
  
  bool eos() const override { return _it == _sources.end(); }
  
  size_t read(void* dest, size_t amount) override
  {
    if (_pristine)
    {
      _onBegin(*_it);
      _pristine = false;
    }
    
    size_t effective = (*_it)->read(dest, amount);
    if ((*_it)->eos())
    {
      _onEnd(*_it);
      ++_it;
      _pristine = true;
    }
    
    return effective;
  }
};

class data_sink
{
  
public:
  virtual size_t write(const void* src, size_t amount) = 0;
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
