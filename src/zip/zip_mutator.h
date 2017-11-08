#pragma once

#include "core/data_source.h"
#include "core/data_pipe.h"
#include "core/data_filter.h"

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
