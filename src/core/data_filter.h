#pragma once

#include "memory_buffer.h"

class unbuffered_data_filter
{
protected:
  virtual void process(const byte* data, size_t amount, size_t effective) = 0;
  
  virtual std::string name() const = 0;
};

class lambda_unbuffered_data_filter : unbuffered_data_filter
{
public:
  using lambda_t = std::function<void(const byte*, size_t, size_t)>;
  lambda_unbuffered_data_filter(std::string name, lambda_t lambda) : _lambda(lambda) { }
private:
  std::string _name;
  lambda_t _lambda;
public:
  void process(const byte* data, size_t amount, size_t effective) override final { _lambda(data, amount, effective); }
  std::string name() const override { return _name; }
};

template<typename T>
class unbuffered_source_filter : public data_source
{
protected:
  data_source* _source;
  T _filter;
public:
  template<typename... Args>
  unbuffered_source_filter(data_source* source, Args... args) : _source(source), _filter(args...) { }
  
  size_t read(byte* dest, size_t amount) override
  {
    size_t read = _source->read(dest, amount);
    TRACE_P("%p: %s_unbuffered_source_filter::read(%lu/%lu)", this, _filter.name().c_str(), read, amount);

    _filter.process(dest, amount, read);
    return read;
  }
  
  T& filter() { return _filter; }
  const T& filter() const { return _filter; }
  
  void setSource(data_source* source) { this->_source = source; }
};

template<typename T>
class unbuffered_sink_filter : public data_sink
{
protected:
  data_sink* _sink;
  T _filter;
public:
  template<typename... Args>
  unbuffered_sink_filter(data_sink* sink, Args... args) : _sink(sink), _filter(args...) { }
  
  size_t write(const byte* src, size_t amount) override
  {
    size_t written = _sink->write(src, amount);
    _filter.process(src, amount, written);
    TRACE_P("%p: %s_unbuffered_sink_filter::read(%lu/%lu)", this, _filter.name().c_str(), written, amount);

    return written;
  }
  
  T& filter() { return _filter; }
  const T& filter() const { return _filter; }
};

class data_filter
{
protected:
  memory_buffer _in;
  memory_buffer _out;

private:
  bool _started;
  bool _finished;
  bool _isEnded;
  
private:
  
public:
  data_filter(size_t inBufferSize, size_t outBufferSize) : _in(inBufferSize), _out(outBufferSize), _started(false), _finished(false), _isEnded(false) { }

  data_filter() : data_filter(KB16, KB16) { }
  data_filter(size_t bufferSize) : data_filter(bufferSize, bufferSize) { }

  virtual ~data_filter()
  {

  }
  
  memory_buffer& in() { return _in; }
  memory_buffer& out() { return _out; }
  
  virtual void init() = 0;
  virtual void process() = 0;
  virtual void finalize() = 0;
  
  void resizeIn(size_t capacity) { _in.resize(capacity); }
  void resizeOut(size_t capacity) { _out.resize(capacity); }
  void resize(size_t capacity) { resizeIn(capacity); resizeOut(capacity); }
  
  void start() { _started = true; }
  void markEnded() { _isEnded = true; }
  void markFinished(bool value = true) { _finished = value; }
  
  bool started() const { return _started; }
  bool finished() const { return _finished; }
  bool ended() const { return _isEnded; }
  
  /* for debugging purposes */
  virtual std::string name() = 0;
};

template<typename F>
class source_filter : public data_source
{
private:
  data_source* _source;
  F _filter;
  
protected:
  
  void fetchInput()
  {
    memory_buffer& in = _filter.in();
    
    if (!in.full())
    {
      size_t effective = _source->read(in.tail(), in.available());
      
#if defined (DEBUG)
      if (effective != END_OF_STREAM)
        TRACE_P("%p: %s_filter_source::fetchInput(%lu/%lu)", this, _filter.name().c_str(), effective, in.available());
      else
        TRACE_P("%p: %s_filter_source::fetchInput(EOS)", this, _filter.name().c_str(), effective, in.available());
#endif
      
      if (effective != END_OF_STREAM)
        in.advance(effective);
      else
        _filter.markEnded();
      
    }
  }
  
  size_t dumpOutput(byte* dest, size_t length)
  {
    memory_buffer& out = _filter.out();
    
    if (!out.empty())
    {
      size_t effective = std::min(out.used(), length);
      std::copy(out.head(), out.head() + effective, dest);
      out.consume(effective);
      
      TRACE_P("%p: %s_filter_source::dumpOutput(%lu/%lu)", this, _filter.name().c_str(), effective, length);
      
      
      return effective;
    }
    
    return 0;
  }
  
  
public:
  template<typename... Args> source_filter(data_source* source, Args... args) : _source(source), _filter(args...) { }
  
  size_t read(byte* dest, size_t amount) override
  {
    if (!_filter.started())
    {
      _filter.init();
      _filter.start();
    }
    
    fetchInput();
    
    if ((/*!_filter.ended() || */!_filter.finished()) && (!_filter.in().empty() || !_filter.out().full()))
      _filter.process();
      
      size_t effective = dumpOutput(dest, amount);
      
      if (_filter.ended() && _filter.finished())
      {
        /* this is needed because additional data could have been buffered, we must discard it */
        _filter.in().consume(_filter.in().size());
        
        _filter.finalize();
      }
    
    if (effective == 0 && _filter.ended() && _filter.in().empty() && _filter.out().empty() && _filter.finished())
      return END_OF_STREAM;
    else
      return effective;
  }
  
  F& filter() { return _filter; }
  const F& filter() const { return _filter; }
};

template<typename F>
class sink_filter : public data_sink
{
private:
  data_sink* _sink;
  F _filter;
  
protected:
  
  size_t fetchInput(const byte* src, size_t length)
  {
    memory_buffer& in = _filter.in();
    
    if (!in.full())
    {
      size_t effective = std::min(in.available(), length);
      std::copy(src, src + effective, in.tail());
      in.advance(effective);
      return effective;
    }
    
    return 0;
  }
  
  size_t dumpOutput()
  {
    memory_buffer& out = _filter.out();
    
    
    if (!out.empty())
    {
      size_t effective = _sink->write(out.head(), out.used());
      out.consume(effective);
      return effective;
    }
    else
    {
      if (_filter.finished())
        return _sink->write(nullptr, END_OF_STREAM);
      else
        return 0;
    }
  }
  
public:
  template<typename... Args> sink_filter(data_sink* sink, Args... args) : _sink(sink), _filter(args...) { }
  
  size_t write(const byte* src, size_t amount) override
  {
    if (!_filter.started())
    {
      _filter.init();
      _filter.start();
    }
    
    size_t effective = 0;
    if (amount != END_OF_STREAM)
    {
      effective = fetchInput(src, amount);
      if (!_filter.finished() && (!_filter.in().empty() || !_filter.out().full()))
        _filter.process();
        dumpOutput();
        }
    else
    {
      _filter.markEnded();
      while (!_filter.finished())
        _filter.process();
        while (dumpOutput() != END_OF_STREAM) ;
      effective = END_OF_STREAM;
    }
    
    if (_filter.ended() && _filter.in().empty() && _filter.out().empty() && _filter.finished())
    {
      _filter.finalize();
    }
    
    return effective;
  }
  
  F& filter() { return _filter; }
  const F& filter() const { return _filter; }
};
