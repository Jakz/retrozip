#pragma once

#include "core/data_source.h"
#include "core/data_pipe.h"
#include "core/data_filter.h"

#include "lzma.h"

namespace options
{
  struct LZMAEncoder
  {
    u32 level;
    bool extreme;
  };
}

namespace compression
{
  template<bool IS_ENCODER>
  class lzma_filter : public data_filter
  {
  private:
    options::LZMAEncoder _options;
    
    lzma_stream _stream;
    lzma_ret _r;
    
    static const char* printableErrorCode(lzma_ret value);
    
  public:
    lzma_filter(size_t bufferSize, const options::LZMAEncoder& options) : data_filter(bufferSize), _stream(LZMA_STREAM_INIT), _options(options) { }
    lzma_filter(size_t bufferSize) : lzma_filter(bufferSize, { 9, true}) { }
    
    void init() override;
    void process() override;
    void finalize() override;
    
    std::string name() override { return "lzma_encoder"; }
  };
  
  using lzma_encoder = lzma_filter<true>;
  using lzma_decoder = lzma_filter<false>;

}
