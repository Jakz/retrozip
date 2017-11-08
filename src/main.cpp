//
//  main.cpp
//  retrozip
//
//  Created by Jack on 26/10/17.
//  Copyright © 2017 Jack. All rights reserved.
//

#include <iostream>
#include "core/header.h"
#include "core/archive.h"
#include "filters/filters.h"
#include "core/data_source.h"
#include "filters/deflate_filter.h"

#include "patch/xdelta3/xdelta3.h"



#define REQUIRE assert

const char* nameForXdeltaReturnValue(int value)
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
    default: return "UNKNOWN";
  }
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
  
  _config.flags = XD3_SEC_DJW | XD3_COMPLEVEL_9;
  
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
  if (_isEnded && _in.empty() && (_stream.avail_in || _stream.buf_avail || _stream.buf_leftavail) && !(_stream.flags & XD3_FLUSH))
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
  
  /*if (!isEncoder)
  {
    for (size_t kk = 0; kk < _in.used() + _stream.total_in; ++kk)
      printf("%02lu ", kk);
    printf("\n");
    for (size_t kk = 0; kk < _in.used() + _stream.total_in; ++kk)
      if (kk < _stream.total_in) printf("   ");
      else printf("%02X ", _in.raw()[kk - _stream.total_in]);

    printf("\n");
  }*/

  _state = FUNCTION(&_stream);
  
  if (!isEncoder)
    effective = effective - _stream.avail_in;

  if (_state == XD3_INPUT || !isEncoder)
  {
    TRACE("%p: xdelta3_%s::process() XD3_INPUT consumed %lu/%lu bytes (avail_in: %lu total_in: %lu)", this, name().c_str(), effective, _in.used(), _stream.avail_in, _stream.total_in);

    _in.consume(effective);
    
    if (_isEnded && _in.empty() && _stream.buf_avail == 0 && _stream.buf_leftover == 0)
    {
      TRACE("%p: xdelta3_%s::process() input was finished so encoding is finished", this, name().c_str());
      _finished = true;
    }
  }
  
  switch (_state)
  {
    case XD3_OUTPUT:
    case XD3_GOTHEADER:
    {
      TRACE("%p: xdelta3_%s::process() %s produced bytes (avail_out: %lu, total_out: %lu)", this, name().c_str(), nameForXdeltaReturnValue(_state), _stream.avail_out, _stream.total_out);
      
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
      const off_t offset = _sourceBlockSize * blockNumber;
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
      TRACE("%p: xdelta3_%s::process() %s", this, name().c_str(), nameForXdeltaReturnValue(_state));
      assert(false);
  }
}

template<xd3_function FUNCTION>
void xdelta3_filter<FUNCTION>::finalize()
{
  xd3_close_stream(&_stream);
  xd3_free_stream(&_stream);
}

#include <sstream>

static std::stringstream ss;
static std::stringstream sc;
void test_xdelta3_encoding(size_t testLength, size_t modificationCount, size_t bufferSize, size_t windowSize, size_t blockSize)
{
  constexpr bool TEST_WITH_REAL_TOOL = false;
  constexpr bool USE_DEFLATER = true;
  
  assert(windowSize >= KB16 && windowSize <= MB16);
  
  if (TEST_WITH_REAL_TOOL)
  {
    /*remove("source.bin");
    remove("input.bin");
    remove("generated.bin");
    remove("patch.xdelta3");*/
  }
  
  sc.str("");
  sc << " (source: " << strings::humanReadableSize(testLength, false) << ", mods: ";
  sc << modificationCount << ", winsize: ";
  sc << strings::humanReadableSize(windowSize, false) << ", blksize: ";
  sc << strings::humanReadableSize(blockSize, false) << ", bufsize: " ;
  sc << strings::humanReadableSize(bufferSize, false) << ")";
  
  //std::cout << ">>>>>>>> TEST" << std::endl << sc.str() << std::endl << std::endl;
  
  byte* bsource = new byte[testLength];
  for (size_t i = 0; i < testLength; ++i) bsource[i] = rand()%256;
  
  byte* binput = new byte[testLength];
  memcpy(binput, bsource, testLength);
  for (size_t i = 0; i < modificationCount; ++i) binput[rand()%(testLength)] = rand()%256;
  
  memory_buffer source(bsource, testLength);
  memory_buffer input(binput, testLength);
  
  memory_buffer sink(testLength >> 1);
  memory_buffer generated(testLength);
  
  {
    source_filter<xdelta3_encoder> encoder(&input, &source, bufferSize, windowSize, blockSize);
    source_filter<compression::deflater_filter> deflater(&encoder, bufferSize);

    passthrough_pipe pipe(USE_DEFLATER ? &deflater : (data_source*)&encoder, &sink, windowSize);
    pipe.process();
  }
  
  //std::cout << std::endl << std::endl << ">>>>>>>> DECODING" << std::endl;
  
  if (TEST_WITH_REAL_TOOL)
  {
    source.rewind();
    input.rewind();
    
    source.serialize(file_handle("source.bin", file_mode::WRITING));
    input.serialize(file_handle("input.bin", file_mode::WRITING));
    
    sink.serialize(file_handle("patch.xdelta3", file_mode::WRITING));
    
    assert(system("/usr/local/bin/xdelta3 -f -d -s source.bin patch.xdelta3 generated.bin") == 0);
    generated.unserialize(file_handle("generated.bin", file_mode::READING));
  }
  else
  {
    sink.rewind();
    
    source_filter<compression::inflater_filter> inflater(&sink, bufferSize);
    source_filter<xdelta3_decoder> decoder(USE_DEFLATER ? (data_source*)&inflater : &sink, &source, bufferSize, windowSize, blockSize);
    passthrough_pipe pipe(&decoder, &generated, windowSize);
    pipe.process();
  }
  

  
  bool success = generated == input;
  
  ss << "Test " << (success ? "success" : "failed") << " ";
  ss << sc.str();

  if (success)
    ss << std::endl;
  else
  {
    size_t length = std::min(generated.size(), input.size());
    size_t min = std::numeric_limits<size_t>::max();
    size_t max = 0;
    
    for (size_t i = 0; i < length; ++i)
    {
      if (input.raw()[i] != generated.raw()[i])
      {
        min = std::min(min, i);
        max = std::max(max, i);
      }
    }
    
    ss << " difference in range [" << min << ", " << max << "]" << std::endl;
  }
}


int main(int argc, const char * argv[])
{
  /*test_xdelta3_encoding(MB1, KB16, MB1, MB1, MB1);
  test_xdelta3_encoding(MB1, KB16, MB1, MB1, KB16);

  test_xdelta3_encoding(MB2, KB16, MB1, MB1, MB1);
  test_xdelta3_encoding(MB1, KB16, MB1, MB2, MB2);*/
  
  size_t steps[] = { KB16, KB32, KB64, KB256, MB1, MB2 };
  size_t dsteps[] = { 16000, 32000, 64000, 256000, 1000000, 2000000 };
  const size_t count = 4;
  
  /*{
    int i = 0;
    size_t modCount = 0;//steps[i] >> 2;

   test_xdelta3_encoding(steps[i] << 1, modCount << 1, steps[i], steps[i], steps[i]);
  }*/

  
  
  for (size_t i = 0; i < count; ++i)
  {
    size_t m = steps[i] >> 2;
    size_t p = steps[i];
    size_t d = dsteps[i];
    
    // size_t testLength, size_t modificationCount, size_t bufferSize, size_t windowSize, size_t blockSize
    
    // tests with all values equal
    test_xdelta3_encoding(p, m, p, p, p);
    // test with doubled source/input
    test_xdelta3_encoding(p << 1, m << 1, p, p, p);
    // test with halved buffer size
    test_xdelta3_encoding(p, m, p >> 1, p, p);
    // test with doubled buffer size
    test_xdelta3_encoding(p, m, p << 1, p, p);
    
    // uneven source size
    test_xdelta3_encoding(d, m, p, p, p);
    // uneven halved buffer size
    test_xdelta3_encoding(p, m, d >> 1, p, p);
    
    // uneven source size smaller than window
    test_xdelta3_encoding(d, m, p, p, p);
    // uneven source size larger than window
    test_xdelta3_encoding(d << 1, m, p, p, p);
    // uneven buffer
    test_xdelta3_encoding(p, m, d, p, p);
    // uneven block size (will be adjusted to power of two)
    test_xdelta3_encoding(p, m, p, p, d);
    // all uneven
    test_xdelta3_encoding(d, m, d, std::max(d, KB16), d);

  
  }
  
  //test_xdelta3_encoding(KB64, 0, KB32, KB16, KB32);

  std::cout << ss.str();
  
  return 0;
  
  
  constexpr size_t SIZE = (MB1);
  
  byte* bsource = new byte[SIZE];
  for (size_t i = 0; i < SIZE; ++i) bsource[i] = rand()%256;
  
  byte* binput = new byte[SIZE];
  memcpy(binput, bsource, SIZE);
  for (size_t i = 0; i < (SIZE >> 4); ++i) binput[rand()%(SIZE)] = rand()%256;

  memory_buffer source(bsource, SIZE);
  memory_buffer input(binput, SIZE);
  

  
  {
    memory_buffer sink;
    sink.ensure_capacity(MB32);
    
    // size_t bufferSize, usize_t xdeltaWindowSize, usize_t sourceBlockSize
    source_filter<xdelta3_encoder> filter(&input, &source, MB1, MB1, MB1);
    
    passthrough_pipe pipe(&filter, &sink, MB1);
    pipe.process();
    
    source.rewind();
    input.rewind();
    
    /*usize_t used = 0;
    sink.reserve(SIZE);
    xd3_encode_memory(input.raw(), SIZE, source.raw(), SIZE, sink.raw(), &used, SIZE, XD3_FLUSH);
    sink.advance(used);*/
    
    source.serialize(file_handle("source.bin", file_mode::WRITING));
    input.serialize(file_handle("input.bin", file_mode::WRITING));

    sink.serialize(file_handle("patch.xdelta3", file_mode::WRITING));
  }
  
  return 0;
}

