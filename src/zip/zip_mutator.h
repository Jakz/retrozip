#pragma once

#include "core/data_source.h"
#include "core/data_pipe.h"

#include <zlib.h>

class data_filter
{
protected:
  memory_buffer _in;
  memory_buffer _out;
  
  bool _started;
  bool _finished;
  bool _isEnded;
  
public:
  data_filter(size_t inBufferSize, size_t outBufferSize) : _in(inBufferSize), _out(outBufferSize), _started(false), _finished(false), _isEnded(false) { }
  
  memory_buffer& in() { return _in; }
  memory_buffer& out() { return _out; }
  
  virtual void init() = 0;
  virtual void process() = 0;
  virtual void finalize() = 0;
  
  void start() { _started = true; }
  void markEnded() { _isEnded = true; }
  
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
    
    if (!_filter.finished() && (!_filter.in().empty() || !_filter.out().full()))
      _filter.process();
    
    size_t effective = dumpOutput(dest, amount);
    
    if (_filter.ended() && _filter.finished())
    {
      _filter.finalize();
    }
    
    if (effective == 0 && _filter.ended() && _filter.in().empty() && _filter.out().empty() && _filter.finished())
      return END_OF_STREAM;
    else
      return effective;
  }
  
  F& filter() { return _filter; }
};

template<typename F>
class buffered_sink_filter : public data_sink
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
  template<typename... Args> buffered_sink_filter(data_sink* sink, Args... args) : _sink(sink), _filter(args...) { }
  
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
};


namespace compression
{
  struct DeflateOptions
  {
    enum class Strategy : int
    {
      DEFAULT = Z_DEFAULT_STRATEGY,
      HUFFMAN_ONLY = Z_HUFFMAN_ONLY,
      RLE = Z_RLE,
      FILTERED = Z_FILTERED
    };
    
    int level;
    int windowSize;
    int memLevel;
    Strategy strategy;
    
    DeflateOptions() : level(Z_DEFAULT_COMPRESSION), windowSize(15), memLevel(8), strategy(Strategy::DEFAULT) { }
    
    int init(z_streamp stream)
    {
      return deflateInit2(stream, level, Z_DEFLATED, -windowSize, memLevel, (int)strategy);
    }
  };
  
  struct InflateOptions
  {
    int windowSize;
    
    InflateOptions() : windowSize(15) { }
    
    int init(z_streamp stream)
    {
      return inflateInit2(stream, -windowSize);
    }
  };
  
  using zlib_compute_function = int(*)(z_streamp, int);
  using zlib_end_function = int(*)(z_streamp);
  
  template<zlib_compute_function computer, zlib_end_function finalizer, typename OPTIONS>
  class zlib_filter : public data_filter
  {
  private:
    z_stream _stream;
    OPTIONS _options;
    
    int _result;
    int _failed;
    
  public:
    zlib_filter(size_t bufferSize) : data_filter(bufferSize, bufferSize) { }
    
    void init() override;
    void process() override;
    void finalize() override;
    
    std::string name() override { return std::is_same<OPTIONS, DeflateOptions>::value ? "deflater" : "inflater"; }
    
  public:
    const z_stream& zstream() { return _stream; }
  };
  
  using deflater_filter = zlib_filter<deflate, deflateEnd, DeflateOptions>;
  using inflater_filter = zlib_filter<inflate, inflateEnd, InflateOptions>;
}
