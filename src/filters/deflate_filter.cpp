#include "deflate_filter.h"

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

#pragma mark deflate_filter
template<zlib_compute_function computer, zlib_end_function finalizer, typename OPTIONS>
void zlib_filter<computer, finalizer, OPTIONS>::init()
{
  _stream.zalloc = Z_NULL;
  _stream.zfree = Z_NULL;
  _stream.opaque = Z_NULL;
  
  _stream.total_out = 0;
  _stream.total_in = 0;
  
  _result = _options.init(&_stream);
  assert(_result == Z_OK);
  
  _failed = false;
  start();
}

template<zlib_compute_function computer, zlib_end_function finalizer, typename OPTIONS>
void zlib_filter<computer, finalizer, OPTIONS>::finalize()
{

}

template<zlib_compute_function computer, zlib_end_function finalizer, typename OPTIONS>
void zlib_filter<computer, finalizer, OPTIONS>::process()
{
  _stream.avail_in = static_cast<uInt>(_in.used());
  _stream.next_in = _in.head();
  
  _stream.avail_out = static_cast<uInt>(_out.available());
  _stream.next_out = _out.tail();
  
  _result = computer(&_stream, ended() ? Z_FINISH : Z_NO_FLUSH);
  
  size_t consumed = _in.used() - _stream.avail_in;
  size_t produced = _out.available() - _stream.avail_out;
  
  if (consumed) //TODO: not necessary, used to skip tracing, just forward 0 in case
    _in.consume(consumed);
  
  if (produced)
    _out.advance(produced);
  
  if (_result >= 0)
    TRACE("%p: %s_filter::process() %s %lu bytes into %lu bytes (in: %lu out: %lu) (%s)",
           this,
           name().c_str(),
            std::is_same<OPTIONS, options::Deflate>::value ? "zipped" : "unzipped",
           consumed,
           produced,
           _stream.total_in,
           _stream.total_out,
           zlib_result_mnemonic(_result)
    );
  
  switch (_result)
  {
    case Z_BUF_ERROR:
    {
      _out.resize(_out.size()*2);
      break;
    }
      
    case Z_STREAM_END:
    {
      //printf("ArchiveStream end\n");
      break;
    }
      
    case Z_NEED_DICT:
    case Z_ERRNO:
    case Z_STREAM_ERROR:
    case Z_DATA_ERROR:
    case Z_MEM_ERROR:
    {
      printf("Failed status: %s\n", zlib_result_mnemonic(_result));
      assert(false);
      _failed = true;
      markFinished();
    }
      
    default:
      break;
  }
  
  markFinished(_result == Z_STREAM_END);
  
  /* discard eventual data still on input buffer */
  /* TODO: this is a sort of hack which relies on the fact that once reached end of the stream 
     the source will be reseeked for next operations */
  if (_result == Z_STREAM_END)
    _in.consume(_in.size());
  
  if (finished())
    finalizer(&_stream);
}

template class compression::zlib_filter<deflate, deflateEnd, options::Deflate>;
template class compression::zlib_filter<inflate, inflateEnd, options::Inflate>;
