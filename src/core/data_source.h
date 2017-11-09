#pragma once

#include "base/common.h"

constexpr size_t END_OF_STREAM = 0xFFFFFFFFFFFFFFFFLL;

struct data_source
{
  virtual ~data_source() { }
  virtual size_t read(byte* dest, size_t amount) = 0;
};

struct data_sink
{
  virtual ~data_sink() { }
  virtual size_t write(const byte* src, size_t amount) = 0;
  
  template<typename T, typename std::enable_if<std::is_base_of<data_sink, T>::value, int>::type = 0>
  T* as() { return static_cast<T*>(this); }
};

struct seekable
{
  virtual void seek(off_t position) = 0;
  virtual off_t tell() const = 0;
  virtual size_t size() const = 0;
};

struct seekable_data_source : public data_source, public seekable { };
struct seekable_data_sink : public data_sink, public seekable { };
struct seekable_data : public data_source, public data_sink, public seekable { };
struct data : public data_source, public data_sink { };

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

#pragma null sink

class null_data_sink : public data_sink
{
private:
  size_t _current;
  size_t _maxAccepted;
public:
  null_data_sink() : _maxAccepted(END_OF_STREAM) { }
  null_data_sink(size_t maxAccepted) : _current(0), _maxAccepted(maxAccepted) { }
  
  size_t write(const byte* src, size_t amount) override
  {
    if (_maxAccepted == END_OF_STREAM)
      return amount;
    else
    {
      size_t effective = std::min(_maxAccepted - _current, amount);
      if (effective > 0)
      {
        _current += effective;
        return effective;
      }
      else
        return END_OF_STREAM; //TODO: should return 0 instead?
    }
  }
};

#pragma multiple source

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

#pragma multiple sink

using sink_factory = std::function<data_sink*()>;

struct multiple_sink_policy
{
  virtual size_t availableToWrite(data_sink* sink, size_t requested) = 0;
  virtual data_sink* buildNext() = 0;
};

class multiple_fixed_size_sink_policy : public multiple_sink_policy
{
private:
  std::vector<size_t> _sizes;
  std::vector<data_sink*> _sinks;
  
  size_t _leftAmount;
  sink_factory _factory;
  
public:
  multiple_fixed_size_sink_policy(sink_factory factory, const std::initializer_list<size_t> sizes) : _sizes(sizes), _factory(factory), _leftAmount(0) { }
  ~multiple_fixed_size_sink_policy() { std::for_each(_sinks.begin(), _sinks.end(), [] (data_sink* sink) { delete sink; }); }
  
  size_t availableToWrite(data_sink* sink, size_t requested) override
  {
    if (_leftAmount == 0)
      return END_OF_STREAM;
    else
    {
      size_t effective = std::min(requested, _leftAmount);
      _leftAmount -= effective;
      return effective;
    }
  }
  
  data_sink* buildNext() override
  {
    if (_sinks.size() < _sizes.size())
    {
      data_sink* sink = _factory();
      _sinks.push_back(sink);
      _leftAmount = _sizes[_sinks.size()-1];
      return sink;
    }
    
    return nullptr;
  }
  
};

class multiple_data_sink : public data_sink
{
private:
  using iterator = std::vector<data_sink*>::const_iterator;

  std::vector<data_sink*> _sinks;
  data_sink* _sink;
  
  multiple_sink_policy* _policy;

public:
  multiple_data_sink(multiple_sink_policy* policy) : _policy(policy), _sink(nullptr) { }
  
  size_t write(const byte* src, size_t amount) override
  {
    if (!_sink)
    {
      _sink = _policy->buildNext();
     
      if (!_sink)
        return END_OF_STREAM;
      else
        _sinks.push_back(_sink);
    }
    
    size_t available = _policy->availableToWrite(_sink, amount);
    
    if (available == END_OF_STREAM)
    {
      _sink = nullptr;
      return 0;
    }
    else
      return _sink->write(src, available);
  }
  
  data_sink* operator[](size_t index) const { return _sinks[index]; }
};
