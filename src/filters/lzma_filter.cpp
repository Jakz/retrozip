#include "core/data_source.h"
#include "core/data_pipe.h"
#include "core/data_filter.h"

#include "lzma_filter.h"

template<bool E>
const char* compression::lzma_filter<E>::printableErrorCode(lzma_ret r)
{
  switch (r)
  {
    case LZMA_OK: return "LZMA_OK";
    case LZMA_STREAM_END: return "LZMA_STREAM_END";
    case LZMA_NO_CHECK: return "LZMA_NO_CHECK";
    case LZMA_UNSUPPORTED_CHECK: return "LZMA_UNSUPPORTED_CHECK";
    case LZMA_GET_CHECK: return "LZMA_GET_CHECK";
    case LZMA_MEM_ERROR: return "LZMA_MEM_ERROR";
    case LZMA_MEMLIMIT_ERROR: return "LZMA_MEMLIMIT_ERROR";
    case LZMA_BUF_ERROR: return "LZMA_BUF_ERROR";
    case LZMA_DATA_ERROR: return "LZMA_DATA_ERROR";
    case LZMA_PROG_ERROR: return "LZMA_PROG_ERROR";
    case LZMA_FORMAT_ERROR: return "LZMA_FORMAT_ERROR";
    case LZMA_OPTIONS_ERROR: return "LZMA_OPTIONS_ERROR";
  }
}

template<bool IS_ENCODER>
void compression::lzma_filter<IS_ENCODER>::init()
{

  //lzma_mt options;
  //lzma_ret r = lzma_stream_encoder_mt(&_stream, const lzma_mt *options)
  
  lzma_ret r;
  
  //TODO: arguments should be adjustable

  if (IS_ENCODER)
    r = lzma_easy_encoder(&_stream, 9 | LZMA_PRESET_EXTREME, LZMA_CHECK_CRC64);
  else
    r = lzma_stream_decoder(&_stream, UINT64_MAX, LZMA_TELL_ANY_CHECK);
  
  assert(r == LZMA_OK);
}

template<bool E>
void compression::lzma_filter<E>::finalize()
{
  lzma_end(&_stream);
}

template<bool IS_ENCODER>
void compression::lzma_filter<IS_ENCODER>::process()
{
  _stream.avail_in = static_cast<size_t>(_in.used());
  _stream.next_in = _in.head();
  
  _stream.avail_out = static_cast<size_t>(_out.available());
  _stream.next_out = _out.tail();
  
  lzma_ret r = lzma_code(&_stream, ended() ? LZMA_FINISH : LZMA_RUN);
  
  size_t consumed = _in.used() - _stream.avail_in;
  size_t produced = _out.available() - _stream.avail_out;
  
  if (consumed) //TODO: not necessary, used to skip tracing, just forward 0 in case
    _in.consume(consumed);
  
  if (produced)
    _out.advance(produced);
  
  if (r != LZMA_OK)
    TRACE("%p: %s::process() %s %lu bytes into %lu bytes (in: %lu out: %lu) (%s)",
          this,
          name().c_str(),
          IS_ENCODER ? "compressed" : "uncompressed",
          consumed,
          produced,
          _stream.total_in,
          _stream.total_out,
          printableErrorCode(r)
    );
  
  assert(r == LZMA_OK || r == LZMA_STREAM_END);
  markFinished(r == LZMA_STREAM_END);
}

template class compression::lzma_filter<true>;
template class compression::lzma_filter<false>;
