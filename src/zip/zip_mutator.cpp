#include "zip_mutator.h"

using namespace compression;

const char* zlib_result_mnemonic(int result)
{
  switch (result)
  {
    case Z_OK: return "Z_OK";
    case Z_STREAM_END: return "Z_STREAM_END";

    case Z_STREAM_ERROR: return "Z_STREAM_ERROR";
    case Z_DATA_ERROR: return "Z_DATA_ERROR";
    case Z_MEM_ERROR: return "Z_MEM_ERROR";
    case Z_BUF_ERROR: return "Z_BUF_ERROR";
    
    default: return "Z_UNKNOWN";
  }
}

deflate_mutator::deflate_mutator(const data_source* source, data_sink* sink, size_t inBufferSize, size_t outBufferSize) :
  data_mutator(source, sink, inBufferSize, outBufferSize)
{
  
}

void deflate_mutator::initialize()
{
  _stream.zalloc = Z_NULL;
  _stream.zfree = Z_NULL;
  _stream.opaque = Z_NULL;
  
  _stream.total_out = 0;
  _stream.total_in = 0;
  
  _result = deflateInit2(&_stream, _options.level, Z_DEFLATED, -_options.windowSize, _options.memLevel, (int)_options.strategy);
  assert(_result == Z_OK);
  
  _failed = false;
  _finished = false;
}

void deflate_mutator::finalize()
{
  _result = deflateEnd(&_stream);
  assert(_result == Z_OK);
}

bool deflate_mutator::mutate()
{
  if ((!_in.empty() && !_out.full()) || !_finished)
  {
    _stream.avail_in = static_cast<uInt>(_in.used());
    _stream.next_in = _in.head();
    
    _stream.avail_out = static_cast<uInt>(_out.available());
    _stream.next_out = _out.tail();
    
    _result = deflate(&_stream, _source->eos() ? Z_FINISH : Z_NO_FLUSH);
    
    size_t consumed = _in.used() - _stream.avail_in;
    size_t produced = _out.available() - _stream.avail_out;
    
    _in.consume(consumed);
    _out.advance(produced);
    
    //if (_result >= 0)
    //  printf("Zipped %lu bytes into %lu bytes (in: %lu out: %lu) (%s)\n", consumed, produced, _stream.total_in, _stream.total_out, zlib_result_mnemonic(_result));
    
    switch (_result)
    {
      case Z_BUF_ERROR:
      {
        _out.resize(_out.size()*2);
        break;
      }
        
      case Z_NEED_DICT:
      case Z_ERRNO:
      case Z_STREAM_ERROR:
      case Z_DATA_ERROR:
      case Z_MEM_ERROR:
      {
        assert(false);
        _failed = true;
        _finished = true;
      }
        
      default:
        break;
    }
    _finished = _result == Z_STREAM_END;
    
    return !_finished;
  }
  
  return false;
}




inflate_mutator::inflate_mutator(const data_source* source, data_sink* sink, size_t inBufferSize, size_t outBufferSize) :
data_mutator(source, sink, inBufferSize, outBufferSize)
{
  
}

void inflate_mutator::initialize()
{
  _stream.zalloc = Z_NULL;
  _stream.zfree = Z_NULL;
  _stream.opaque = Z_NULL;
  
  _stream.total_out = 0;
  _stream.total_in = 0;
  
  _result = inflateInit2(&_stream, -_options.windowSize);
  assert(_result == Z_OK);
  
  _failed = false;
}

void inflate_mutator::finalize()
{
  _result = inflateEnd(&_stream);
  assert(_result == Z_OK);
}

bool inflate_mutator::mutate()
{
  if ((!_in.empty() && !_out.full()) || !_finished)
  {
    _stream.avail_in = static_cast<uInt>(_in.used());
    _stream.next_in = _in.head();
    
    _stream.avail_out = static_cast<uInt>(_out.available());
    _stream.next_out = _out.tail();
  
    _result = inflate(&_stream, _source->eos() ? Z_FINISH : Z_NO_FLUSH);
    
    size_t consumed = _in.used() - _stream.avail_in;
    size_t produced = _out.available() - _stream.avail_out;
    
    _in.consume(consumed);
    _out.advance(produced);
    
    //if (_result >= 0)
    //  printf("Unzipped %lu bytes into %lu bytes (in: %lu out: %lu) (%s)\n", consumed, produced, _stream.total_in, _stream.total_out, zlib_result_mnemonic(_result));
    
    switch (_result)
    {
      case Z_BUF_ERROR:
      {
        _out.resize(_out.size()*2);
        break;
      }
      
      case Z_NEED_DICT:
      case Z_ERRNO:
      case Z_STREAM_ERROR:
      case Z_DATA_ERROR:
      case Z_MEM_ERROR:
      {
        assert(false);
        _failed = true;
        _finished = true;
      }

      default:
        break;
    }
    
    _finished = _result == Z_STREAM_END;
    
    return !_finished;
  }
  
  return false;
}
