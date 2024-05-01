#include "tbx/streams/data_source.h"
#include "tbx/streams/data_pipe.h"
#include "tbx/streams/data_filter.h"

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
  //TODO: checks disabled since checksum are computed by archive itself, but they should be adjustable on filter

  if (IS_ENCODER)
  {
    lzma_mt options = {
      .flags = 0,
      .block_size = 0,
      .timeout = 0,
      
      .preset = _options.level | (_options.extreme ? LZMA_PRESET_EXTREME : 0),
      .filters = NULL,
      

      .check = LZMA_CHECK_NONE,
    };

    options.preset = LZMA_PRESET_DEFAULT;
    options.threads = lzma_cputhreads() - 2;

    _r = lzma_stream_encoder_mt(&_stream, &options);
    //lzma_easy_encoder(&_stream, _options.level | (_options.extreme ? LZMA_PRESET_EXTREME : 0), /*LZMA_CHECK_CRC64*/LZMA_CHECK_NONE);
  }
  else
    _r = lzma_stream_decoder(&_stream, UINT64_MAX, /*LZMA_TELL_ANY_CHECK*/LZMA_TELL_NO_CHECK);
  
  TRACE("%p: %s::init()", this, name().c_str());
  
  assert(_r == LZMA_OK);
}

template<bool E>
void compression::lzma_filter<E>::finalize()
{
  lzma_end(&_stream);
}

template<bool IS_ENCODER>
void compression::lzma_filter<IS_ENCODER>::process()
{
  _stream.avail_in = _in.used();
  _stream.next_in = _in.head();
  
  _stream.avail_out = _out.available();
  _stream.next_out = _out.tail();
  
  _r = lzma_code(&_stream, ended() ? LZMA_FINISH : LZMA_RUN);
  
  size_t consumed = _in.used() - _stream.avail_in;
  size_t produced = _out.available() - _stream.avail_out;
  
  if (consumed) //TODO: not necessary, used to skip tracing, just forward 0 in case
    _in.consume(consumed);
  
  if (produced)
    _out.advance(produced);
  
  if (_r != LZMA_OK)
    TRACE("%p: %s::process() %s %lu bytes into %lu bytes (in: %lu out: %lu) (%s)",
          this,
          name().c_str(),
          IS_ENCODER ? "compressed" : "uncompressed",
          consumed,
          produced,
          _stream.total_in,
          _stream.total_out,
          printableErrorCode(_r)
    );
  
  //TODO: add additional check statuses?
  assert(_r == LZMA_OK || _r == LZMA_STREAM_END || _r == LZMA_NO_CHECK);
  markFinished(_r == LZMA_STREAM_END);
  if (_r == LZMA_STREAM_END)
    markEnded();
  
  TRACE_IF(_r == LZMA_STREAM_END, "%p: %s::process() finished", this, name().c_str());
}

template class compression::lzma_filter<true>;
template class compression::lzma_filter<false>;
