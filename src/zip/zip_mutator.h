#pragma once

#include "core/data_source.h"

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

  class deflate_mutator : public data_mutator
  {
  private:
    DeflateOptions _options;
    z_stream _stream;
    
    int _result;
    bool _failed;
    bool _finished;
    
  public:
    deflate_mutator(const data_source* source, data_sink* sink, size_t inBufferSize, size_t outBufferSize);
    
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
    inflate_mutator(const data_source* source, data_sink* sink, size_t inBufferSize, size_t outBufferSize);
    
    const z_stream& zstream() { return _stream; }
    
    void initialize() override;
    void finalize() override;
    bool mutate() override;
  };
  
  using zip_mutator = deflate_mutator;
}
