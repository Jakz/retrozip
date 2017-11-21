#pragma once

#include "core/data_source.h"
#include "core/data_pipe.h"
#include "core/data_filter.h"

#include "lzma.h"

namespace compression
{
  class lzma_filter : public data_filter
  {
  private:
    lzma_stream _stream;
    
  public:
    lzma_filter() : _stream(LZMA_STREAM_INIT) { }
    
    virtual void finalize()
    {
      lzma_end(&_stream);
    }
  };

}
