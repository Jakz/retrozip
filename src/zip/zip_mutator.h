#pragma once

#include "core/data_source.h"
#include "core/data_pipe.h"

#include <zlib.h>

class buffered_filter
{
protected:
  memory_buffer _in;
  memory_buffer _out;
  
  bool _started;
  bool _finished;
  bool _isEnded;
  
  virtual void init() = 0;
  virtual void process() = 0;
  virtual void finalize() = 0;
  
public:
  buffered_filter(size_t inBufferSize, size_t outBufferSize) : _in(inBufferSize), _out(outBufferSize), _started(false), _finished(false), _isEnded(false) { }
};

template<typename F>
class buffered_source_filter : public data_source, public F
{
private:
  data_source* _source;
  
protected:
  
  void fetchInput()
  {
    TRACE_P("%p: source_filter::fetchInput()", this);
    
    if (!this->_in.full())
    {
      size_t effective = _source->read(this->_in.tail(), this->_in.available());
      
      if (effective != END_OF_STREAM)
        this->_in.advance(effective);
      else
        this->_isEnded = true;
        
    }
  }
  
  size_t dumpOutput(byte* dest, size_t length)
  {
    TRACE_P("%p: source_filter::dumpOutput(%lu)", this, length);
    
    if (!this->_out.empty())
    {
      size_t effective = std::min(this->_out.used(), length);
      std::copy(this->_out.head(), this->_out.head() + effective, dest);
      this->_out.consume(effective);
      return effective;
    }
    
    return 0;
  }
  
public:
  template<typename... Args> buffered_source_filter(data_source* source, Args... args) : _source(source), F(args...) { }
  
  size_t read(byte* dest, size_t amount) override
  {
    if (!this->_started)
    {
      this->init();
      this->_started = true;
    }
    
    fetchInput();
    
    if (!this->_finished && (!this->_in.empty() || !this->_out.full()))
      this->process();
    
    size_t effective = dumpOutput(dest, amount);
    
    if (this->_isEnded && this->_finished)
    {
      this->finalize();
    }
    
    if (effective == 0 && this->_isEnded && this->_in.empty() && this->_out.empty() && this->_finished)
      return END_OF_STREAM;
    else
      return effective;
  }
};

template<typename F>
class buffered_sink_filter : public data_sink, public F
{
private:
  data_sink* _sink;
  
protected:
  
  size_t fetchInput(const byte* src, size_t length)
  {
    if (!this->_in.full())
    {
      size_t effective = std::min(this->_in.available(), length);
      std::copy(src, src + effective, this->_in.tail());
      this->_in.advance(effective);
      return effective;
    }
    
    return 0;
  }
  
  size_t dumpOutput()
  {
    if (!this->_out.empty())
    {
      size_t effective = _sink->write(this->_out.head(), this->_out.used());
      this->_out.consume(effective);
      return effective;
    }
    else
    {
      if (this->_finished)
        return _sink->write(nullptr, END_OF_STREAM);
      else
        return 0;
    }
  }
  
public:
  template<typename... Args> buffered_sink_filter(data_sink* sink, Args... args) : _sink(sink), F(args...) { }
  
  size_t write(const byte* src, size_t amount) override
  {
    if (!this->_started)
    {
      this->init();
      this->_started = true;
    }
    
    size_t effective = 0;
    if (amount != END_OF_STREAM)
    {
      effective = fetchInput(src, amount);
      if (!this->_finished && (!this->_in.empty() || !this->_out.full()))
        this->process();
      dumpOutput();
    }
    else
    {
      this->_isEnded = true;
      while (!this->_finished)
        this->process();
      while (dumpOutput() != END_OF_STREAM) ;
      effective = END_OF_STREAM;
    }
    
    if (this->_isEnded && this->_in.empty() && this->_out.empty() && this->_finished)
    {
      this->finalize();
    }
    
    return effective;
  }
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
  class zlib_filter : public buffered_filter
  {
  private:
    z_stream _stream;
    OPTIONS _options;
    
    int _result;
    int _failed;
    
  protected:
    zlib_filter(size_t bufferSize) : buffered_filter(bufferSize, bufferSize) { }
    
    void init() override;
    void process() override;
    void finalize() override;
    
  public:
    const z_stream& zstream() { return _stream; }
  };
  
  using deflater_filter = zlib_filter<deflate, deflateEnd, DeflateOptions>;
  using inflater_filter = zlib_filter<inflate, inflateEnd, InflateOptions>;
}
