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
  
  virtual void init() = 0;
  virtual void process() = 0;
  
public:
  buffered_filter(size_t inBufferSize, size_t outBufferSize) : _in(inBufferSize), _out(outBufferSize), _started(false), _finished(false) { }
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
  };
  
  struct InflateOptions
  {
    int windowSize;
    
    InflateOptions() : windowSize(15) { }
  };
  
  class deflater_filter : public buffered_filter
  {
  private:
    z_stream _stream;
    DeflateOptions _options;
    
    int _result;
    int _failed;
    
  protected:
    deflater_filter(size_t bufferSize) : buffered_filter(bufferSize, bufferSize) { }
    
    void init() override;
    void process() override;
  };
  
  inline void deflater_filter::init()
  {
    _stream.zalloc = Z_NULL;
    _stream.zfree = Z_NULL;
    _stream.opaque = Z_NULL;
    
    _stream.total_out = 0;
    _stream.total_in = 0;
    
    _result = deflateInit2(&_stream, _options.level, Z_DEFLATED, -_options.windowSize, _options.memLevel, (int)_options.strategy);
    assert(_result == Z_OK);
    
    _failed = false;
    _started = true;
  }
  
  inline void deflater_filter::process() { }
  
  class deflate_source : public data_source
  {
  private:
    data_source* _source;
    memory_buffer _in, _out;
    
    DeflateOptions _options;
    
    bool _finished;
    bool _started;
    bool _failed;
    int _result;
    
    z_stream _stream;
    
  private:
    void fetchInput();
    size_t dumpOutput(byte* dest, size_t length);
    
  public:
    deflate_source(data_source* source, size_t bufferSize);
    
    bool eos() const override;
    size_t read(void* dest, size_t amount) override;
    
    const z_stream& zstream() { return _stream; }
  };
  
  class inflate_source : public data_source
  {
  private:
    data_source* _source;
    memory_buffer _in, _out;
    
    InflateOptions _options;
    
    bool _finished;
    bool _started;
    bool _failed;
    int _result;
    
    z_stream _stream;
    
  private:
    void fetchInput();
    size_t dumpOutput(byte* dest, size_t length);
    
  public:
    inflate_source(data_source* source, size_t bufferSize);
    
    bool eos() const override;
    size_t read(void* dest, size_t amount) override;
    
    const z_stream& zstream() { return _stream; }
  };
}
