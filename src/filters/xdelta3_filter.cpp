#include "xdelta3_filter.h"


template<xd3_function FUNCTION>
const char* xdelta3_filter<FUNCTION>::printableErrorCode(int value)
{
  switch (value)
  {
    case XD3_INPUT: return "XD3_INPUT";
    case XD3_OUTPUT: return "XD3_OUTPUT";
    case XD3_GETSRCBLK: return "XD3_GETSRCBLK";
    case XD3_GOTHEADER: return "XD3_GOTHEADER";
    case XD3_WINSTART: return "XD3_WINSTART";
    case XD3_WINFINISH: return "XD3_WINFINISH";
    case XD3_TOOFARBACK: return "XD3_TOOFARBACK";
    case XD3_INTERNAL: return "XD3_INTERNAL";
    case XD3_INVALID_INPUT: return "XD3_INVALID_INPUT";
    case XD3_NOSECOND: return "XD3_NOSECOND";
    case XD3_UNIMPLEMENTED: return "XD3_UNIMPLEMENTED";
    case ENOSPC: return "ENOSPC";
    case ENOMEM: return "ENOMEM";
    default: return "UNKNOWN";
  }
}


template<xd3_function FUNCTION>
void xdelta3_filter<FUNCTION>::init()
{
  memset(&_stream, 0, sizeof(_stream));
  memset(&_config, 0, sizeof(_config));
  memset(&_xsource, 0, sizeof(_xsource));
  
  /* configuration */
  xd3_init_config(&_config, 0);
  
  _config.winsize = _windowSize;
  _config.sprevsz = utils::nextPowerOfTwo(_windowSize >> 2);
  _config.getblk = getBlockCallback;
  
  _config.flags = XD3_SEC_LZMA/*XD3_SEC_DJW*/ | XD3_COMPLEVEL_9;
  
  int r = xd3_config_stream(&_stream, &_config);
  assert(r == 0);
  
  
  /* setting source */
  _sourceBlockSize = std::min(_sourceBlockSize, _source->size());
  
  _xsource.blksize = _sourceBlockSize;
  _xsource.max_winsize = _sourceBlockSize;
  
  _xsource.onblk = 0;
  _xsource.curblkno = 0;
  _xsource.curblk = nullptr;
  
  _xsource.ioh = this;
  
  r = xd3_set_source(&_stream, &_xsource);
  assert(r == 0);
  
  /* this is required because block size must be a power of two and xd3_set_source
   adjusts it in case without signalling any error */
  _sourceBlockSize = _xsource.blksize;
  _sourceBuffer.resize(_sourceBlockSize);
  //TODO: choose which policy about backward matching, so how much far back you can seek in source
  
  _state = XD3_INPUT;
}

template<xd3_function FUNCTION>
void xdelta3_filter<FUNCTION>::process()
{
  if (ended() && _in.empty() && (_stream.avail_in || _stream.buf_avail || _stream.buf_leftavail) && !(_stream.flags & XD3_FLUSH))
  {
    TRACE("%p: xdelta3_%s::process() flush request (setting XD3_FLUSH flag)", this, name().c_str());
    xd3_set_flags(&_stream, _stream.flags | XD3_FLUSH);
  }
  
  usize_t effective = xd3_min(_stream.winsize, _in.used());
  
  if (_state == XD3_INPUT || !isEncoder)
  {
    if (effective > 0)
    {
      xd3_avail_input(&_stream, _in.head(), effective);
    }
    else
      xd3_avail_input(&_stream, _in.head(), 0);
  }

  _state = FUNCTION(&_stream);
  
  if (!isEncoder)
    effective = effective - _stream.avail_in;
  
  if (_state == XD3_INPUT || !isEncoder)
  {
    TRACE("%p: xdelta3_%s::process() XD3_INPUT consumed %lu/%lu bytes (avail_in: %lu total_in: %lu)", this, name().c_str(), effective, _in.used(), _stream.avail_in, _stream.total_in);
    
    _in.consume(effective);
    
    if (ended() && _in.empty() && _stream.buf_avail == 0 && _stream.buf_leftover == 0)
    {
      TRACE("%p: xdelta3_%s::process() input was finished so encoding is finished", this, name().c_str());
      markFinished();
    }
  }
  
  switch (_state)
  {
    case XD3_OUTPUT:
    case XD3_GOTHEADER:
    {
      TRACE("%p: xdelta3_%s::process() %s produced bytes (avail_out: %lu, total_out: %lu)", this, name().c_str(), printableErrorCode(_state), _stream.avail_out, _stream.total_out);
      
      size_t produced = _stream.avail_out;
      
      while (produced > _out.available())
        _out.resize(_out.capacity()*2);
      
      memcpy(_out.tail(), _stream.next_out, produced);
      _out.advance(produced);
      
      xd3_consume_output(&_stream);
      break;
    }
      
    case XD3_INPUT:
      TRACE("%p: xdelta3_%s::process() XD3_INPUT awaiting more input (total_out: %lu)", this, name().c_str(), _stream.total_out);
      break;
      
      /*case XD3_GOTHEADER:
       {
       printf("%p: xdelta3_%s::process() XD3_GOTHEADER\n", this, name);
       break;
       }*/
      
    case XD3_WINSTART:
    {
      TRACE("%p: xdelta3_%s::process() XD3_WINSTART started window (avail_in: %lu total_in: %lu)", this, name().c_str(), _stream.avail_in, _stream.total_in);
      break;
    }
      
    case XD3_WINFINISH:
    {
      TRACE("%p: xdelta3_%s::process() XD3_WINFINISH finished window", this, name().c_str());
      break;
    }
      
    case XD3_GETSRCBLK:
    {
      TRACE("%p: xdelta3_%s::process() XD3_GETSRCBLK block request %lu", this, name().c_str(), _xsource.getblkno);
      
      assert(_sourceBuffer.capacity() >= _sourceBlockSize);
      
      const xoff_t blockNumber = _xsource.getblkno;
      const roff_t offset = _sourceBlockSize * blockNumber;
      const usize_t size = std::min(_sourceBlockSize, (usize_t)(_source->size() - offset));
      
      _source->seek(offset);
      _source->read(_sourceBuffer.head(), size);
      
      _xsource.onblk = size;
      _xsource.curblkno = blockNumber;
      _xsource.curblk = _sourceBuffer.head();
      
      break;
    }
      
    case XD3_INVALID_INPUT:
    {
      TRACE("%p: xdelta3_%s::process() invalid input: %s", this, name().c_str(), _stream.msg);
      assert(false);
      break;
    }
      
    default:
      TRACE("%p: xdelta3_%s::process() %s", this, name().c_str(), printableErrorCode(_state));
      assert(false);
  }
}

template<xd3_function FUNCTION>
void xdelta3_filter<FUNCTION>::finalize()
{
  xd3_close_stream(&_stream);
  xd3_free_stream(&_stream);
  //TODO: we should cache with tell on init() and restore here instead that blindly rewind
  _source->rewind();
}

template class xdelta3_filter<xd3_encode_input>;
template class xdelta3_filter<xd3_decode_input>;
