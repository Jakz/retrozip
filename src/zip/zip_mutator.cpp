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

#pragma mark deflate_source
void deflate_source::fetchInput()
  {
    if (!_in.full())
    {
      size_t effective = _source->read(_in.tail(), _in.available());
      _in.advance(effective);
    }
  }
  
size_t deflate_source::dumpOutput(byte* dest, size_t length)
{
  if (!_out.empty())
  {
    size_t effective = std::min(_out.used(), length);
    std::copy(_out.head(), _out.head() + effective, dest);
    _out.consume(effective);
    return effective;
  }
  
  return 0;
}
  
deflate_source::deflate_source(data_source* source, size_t bufferSize) :
_source(source), _in(bufferSize), _out(bufferSize),
_finished(false), _started(false), _failed(false)
{ }

bool deflate_source::eos() const
{
  return _in.empty() && _out.empty() && _finished;
}

size_t deflate_source::read(void* dest, size_t amount)
{
  if (!_started)
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
  
  fetchInput();
  
  if (!_finished && (!_in.empty() || !_out.full()))
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
    
    if (_finished)
      deflateEnd(&_stream);
  }
  
  return dumpOutput((byte*)dest, amount);
}

#pragma mark inflate_source
void inflate_source::fetchInput()
{
  if (!_in.full())
  {
    size_t effective = _source->read(_in.tail(), _in.available());
    _in.advance(effective);
  }
}

size_t inflate_source::dumpOutput(byte* dest, size_t length)
{
  if (!_out.empty())
  {
    size_t effective = std::min(_out.used(), length);
    std::copy(_out.head(), _out.head() + effective, dest);
    _out.consume(effective);
    return effective;
  }
  
  return 0;
}

inflate_source::inflate_source(data_source* source, size_t bufferSize) :
_source(source), _in(bufferSize), _out(bufferSize),
_finished(false), _started(false), _failed(false)
{ }

bool inflate_source::eos() const
{
  return _in.empty() && _out.empty() && _finished;
}

size_t inflate_source::read(void* dest, size_t amount)
{
  if (!_started)
  {
    _stream.zalloc = Z_NULL;
    _stream.zfree = Z_NULL;
    _stream.opaque = Z_NULL;
    
    _stream.total_out = 0;
    _stream.total_in = 0;
    
    _result = inflateInit2(&_stream, -_options.windowSize);
    assert(_result == Z_OK);
    
    _failed = false;
    _started = true;
  }
  
  fetchInput();
  
  if (!_finished && !_in.empty() && !_out.full())
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
    
    if (_finished)
      inflateEnd(&_stream);
  }

  return dumpOutput((byte*)dest, amount);
}

