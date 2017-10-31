#pragma once

#include "core/data_source.h"
#include "core/data_pipe.h"

#include <zlib.h>


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
  
 /* class deflate_mutator : public data_mutator
  {
  private:
    DeflateOptions _options;
    z_stream _stream;
    
    int _result;
    bool _failed;
    bool _finished;
    
  public:
    deflate_mutator(data_source* source, data_sink* sink, size_t inBufferSize, size_t outBufferSize);
    
    const z_stream& zstream() { return _stream; }
    
    void initialize() override;
    void finalize() override;
    bool mutate() override;
  };
  
  class inflate_mutator : public data_mutator
  {
  private:
    InflateOptions _options;
    z_stream _stream;
    
    int _result;
    bool _failed;
    bool _finished;
    
  public:
    inflate_mutator(data_source* source, data_sink* sink, size_t inBufferSize, size_t outBufferSize);
    
    const z_stream& zstream() { return _stream; }
    
    void initialize() override;
    void finalize() override;
    bool mutate() override;
  };
  
  using zip_mutator = deflate_mutator;*/
  
  
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
