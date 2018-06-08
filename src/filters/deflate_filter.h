#pragma once

#include "tbx/streams/data_source.h"
#include "tbx/streams/data_pipe.h"
#include "tbx/streams/data_filter.h"

#include <zlib.h>

namespace options
{
  struct Deflate
  {
    enum class Strategy : int
    {
      DEFAULT = Z_DEFAULT_STRATEGY,
      HUFFMAN_ONLY = Z_HUFFMAN_ONLY,
      RLE = Z_RLE,
      FILTERED = Z_FILTERED
    };
    
    int level; /* 0-9 */
    int windowSize; /* 9-15 */
    int memLevel; /* 1-9 */
    Strategy strategy;
    
    Deflate() : level(Z_DEFAULT_COMPRESSION), windowSize(15), memLevel(8), strategy(Strategy::DEFAULT)
    {
    
    }
    
    int init(z_streamp stream)
    {
      return deflateInit2(stream, level, Z_DEFLATED, -windowSize, memLevel, (int)strategy);
    }
  };
  
  struct Inflate
  {
    int windowSize;
    
    Inflate() : windowSize(15) { }
    
    int init(z_streamp stream)
    {
      return inflateInit2(stream, -windowSize);
    }
  };
}

namespace compression
{
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
    
    std::string name() override { return std::is_same<OPTIONS, options::Deflate>::value ? "deflater" : "inflater"; }
    
  public:
    const z_stream& zstream() { return _stream; }
  };
  
  using deflater_filter = zlib_filter<deflate, deflateEnd, options::Deflate>;
  using inflater_filter = zlib_filter<inflate, inflateEnd, options::Inflate>;
}
