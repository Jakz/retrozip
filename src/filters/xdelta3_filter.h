#pragma once

#include "core/data_filter.h"
#include "patch/xdelta3/xdelta3.h"

namespace options
{
  class Xdelta3
  {
    enum class Compression
    {
      NONE,
      STATIC_HUFFMAN,
      ADAPTIVE_HUFFMAN,
      LZMA
    };
    
    using Level = u32;
    
  private:
    size_t windowSize;
    size_t sourceBlockSize;
  };
}

using xd3_function = int (*) (xd3_stream*);

template<xd3_function FUNCTION>
class xdelta3_filter : public data_filter
{
private:
  seekable_data_source* _source;
  memory_buffer _sourceBuffer;
  
  xd3_stream _stream;
  xd3_config _config;
  xd3_source _xsource;
  int _state;
  
  usize_t _windowSize;
  usize_t _sourceBlockSize;
  
  static int getBlockCallback(xd3_stream *stream, xd3_source *source, xoff_t blkno);
  
  static constexpr bool isEncoder = FUNCTION == xd3_encode_input;
  
  static const char* printableErrorCode(int value);
  
public:
  xdelta3_filter(seekable_data_source* source, size_t bufferSize, usize_t xdeltaWindowSize, usize_t sourceBlockSize) :
  data_filter(bufferSize, bufferSize), _source(source),
  _windowSize(xdeltaWindowSize), _sourceBuffer(sourceBlockSize), _sourceBlockSize(sourceBlockSize) { }
  
  void init() override;
  void process() override;
  void finalize() override;
  
  std::string name() override { return isEncoder ? "encoder" : "decoder"; }
};

template<xd3_function FUNCTION>
int xdelta3_filter<FUNCTION>::getBlockCallback(xd3_stream *stream, xd3_source *source, xoff_t blkno)
{
  xdelta3_filter* filter = (xdelta3_filter*) source->ioh;
  
  TRACE("%p: xdelta3_%s::getBlockCallback() XD3_GETSRCBLK block request %lu", filter, filter->name().c_str(), filter->_xsource.getblkno);
  
  assert(filter->_sourceBuffer.capacity() >= filter->_sourceBlockSize);
  
  const xoff_t blockNumber = filter->_xsource.getblkno;
  const off_t offset = filter->_sourceBlockSize * blockNumber;
  const usize_t size = filter->_source->size() < offset ? 0 : std::min(filter->_sourceBlockSize, (usize_t)(filter->_source->size() - offset));
  
  filter->_source->seek(offset);
  filter->_source->read(filter->_sourceBuffer.head(), size);
  
  filter->_xsource.onblk = size;
  filter->_xsource.curblkno = blockNumber;
  filter->_xsource.curblk = filter->_sourceBuffer.head();
  
  return 0;
}

using xdelta3_encoder = xdelta3_filter<xd3_encode_input>;
using xdelta3_decoder = xdelta3_filter<xd3_decode_input>;
