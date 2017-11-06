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
#include "core/filters.h"
#include "core/data_source.h"
#include "zip/zip_mutator.h"

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
class xdelta3_filter : public buffered_filter
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
  
public:
  xdelta3_filter(seekable_data_source* source, size_t bufferSize, usize_t xdeltaWindowSize, usize_t sourceBlockSize) :
    buffered_filter(bufferSize, bufferSize), _source(source),
    _windowSize(xdeltaWindowSize), _sourceBuffer(sourceBlockSize), _sourceBlockSize(sourceBlockSize) { }
  
  void init() override;
  void process() override;
  void finalize() override;
};

template<xd3_function FUNCTION>
int xdelta3_filter<FUNCTION>::getBlockCallback(xd3_stream *stream, xd3_source *source, xoff_t blkno)
{
  xdelta3_filter* filter = (xdelta3_filter*) source->ioh;
  
  printf("%p: xdelta3::getBlockCallback block request %lu\n", filter, filter->_xsource.getblkno);
  
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
  _config.sprevsz = _windowSize >> 2;
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
  
  _state = XD3_INPUT;
}

template<xd3_function FUNCTION>
void xdelta3_filter<FUNCTION>::process()
{
  if (_isEnded && _in.empty() && !(_stream.flags & XD3_FLUSH))
  {
    printf("%p: xdelta3::process flush request (setting XD3_FLUSH flag)\n", this);
    xd3_set_flags(&_stream, _stream.flags | XD3_FLUSH);
  }
  
  usize_t effective = xd3_min(_stream.winsize, _in.used());
  
  if (_state == XD3_INPUT)
  {
    if (effective > 0)
    {
      xd3_avail_input(&_stream, _in.head(), effective);
    
      static size_t ___total_in = 0;
      ___total_in += effective;
      printf("%p: xdelta3::process consumed %lu bytes (XD3_INPUT) (total: %lu)\n", this, effective, ___total_in);
    }
    else
      xd3_avail_input(&_stream, _in.head(), 0);
  }

  _state = xd3_encode_input(&_stream);

  if (_state == XD3_INPUT)
    _in.consume(effective);

  printf("%p: xdelta3::process %s\n", this, nameForXdeltaReturnValue(_state));
  
  switch (_state)
  {
    case XD3_OUTPUT:
    {
      printf("%p: xdelta3::process produced bytes (avail_out: %lu, total_out: %lu) (XD3_OUTPUT)\n", this, _stream.avail_out, _stream.total_out);
      
      size_t produced = _stream.avail_out;
      
      while (produced > _out.available())
        _out.resize(_out.capacity()*2);
      
      memcpy(_out.tail(), _stream.next_out, produced);
      _out.advance(produced);
      
      xd3_consume_output(&_stream);
      break;
    }
      
    case XD3_INPUT:
      break;
      
    case XD3_GOTHEADER:
    {
      printf("%p: xdelta3::process got %s\n", this, nameForXdeltaReturnValue(_state));
      break;
    }
      
    case XD3_WINSTART:
    {
      printf("%p: xdelta3::process started window (avail_in: %lu) (XD3_WINSTART)\n", this, _stream.avail_in);
      break;
    }
      
    case XD3_WINFINISH:
    {
      printf("%p: xdelta3::process finished window (XD3_WINFINISH)\n", this);
      
      if (_isEnded && _in.empty() && _stream.buf_avail == 0 && _stream.buf_leftover == 0)
      {
        printf("%p: xdelta::process input was finished so encoding is finished\n", this);
        _finished = true;
      }
      
      break;
    }
      
    case XD3_GETSRCBLK:
    {
      printf("%p: xdelta3::process block request %lu (XD3_GETSRCBLK)\n", this, _xsource.getblkno);
      
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
      
    default:
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
void test_xdelta3_encoding(size_t testLength, size_t modificationCount, size_t bufferSize, size_t windowSize, size_t blockSize)
{
  assert(windowSize >= KB16 && windowSize <= MB16);
  
  remove("source.bin");
  remove("input.bin");
  remove("generated.bin");
  remove("patch.xdelta3");
  
  byte* bsource = new byte[testLength];
  for (size_t i = 0; i < testLength; ++i) bsource[i] = rand()%256;
  
  byte* binput = new byte[testLength];
  memcpy(binput, bsource, testLength);
  for (size_t i = 0; i < modificationCount; ++i) binput[rand()%(testLength)] = rand()%256;
  
  memory_buffer source(bsource, testLength);
  memory_buffer input(binput, testLength);
  
  memory_buffer sink(testLength >> 1);
  
  buffered_source_filter<xdelta3_encoder> filter(&input, &source, bufferSize, windowSize, blockSize);

  passthrough_pipe pipe(&filter, &sink, windowSize);
  pipe.process();
  
  source.rewind();
  input.rewind();

  source.serialize(file_handle("source.bin", file_mode::WRITING));
  input.serialize(file_handle("input.bin", file_mode::WRITING));
  
  sink.serialize(file_handle("patch.xdelta3", file_mode::WRITING));
  
  system("/usr/local/bin/xdelta3 -f -d -s source.bin patch.xdelta3 generated.bin");
  
  memory_buffer generated;
  generated.unserialize(file_handle("generated.bin", file_mode::READING));
  
  bool success = generated == input;
  
  ss << "Test " << (success ? "success" : "failed") << " (source: ";
  ss << strings::humanReadableSize(testLength, false) << ", mods: ";
  ss << modificationCount << ", winsize: ";
  ss << strings::humanReadableSize(windowSize, false) << ", blksize: ";
  ss << strings::humanReadableSize(blockSize, false) << ", bufsize: " ;
  ss << strings::humanReadableSize(bufferSize, false) << ")";

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
  
  // size_t testLength, size_t modificationCount, size_t bufferSize, size_t windowSize, size_t blockSize
  
  for (size_t i = 0; i < count; ++i)
  {
    size_t modCount = steps[i] >> 2;
    
    // tests with all values equal
    test_xdelta3_encoding(steps[i], modCount, steps[i], steps[i], steps[i]);
    // test with doubled source/input
    test_xdelta3_encoding(steps[i] << 1, modCount << 1, steps[i], steps[i], steps[i]);
    // test with halved buffer size
    test_xdelta3_encoding(steps[i], modCount, steps[i] >> 1, steps[i], steps[i]);
    // test with doubled buffer size
    test_xdelta3_encoding(steps[i], modCount, steps[i] << 1, steps[i], steps[i]);

    // uneven source size
    test_xdelta3_encoding(dsteps[i], modCount, steps[i], steps[i], steps[i]);
    // uneven halved buffer size
    test_xdelta3_encoding(steps[i], modCount, dsteps[i] >> 1, steps[i], steps[i]);
    
    // uneven source size smaller than window
    test_xdelta3_encoding(dsteps[i], modCount, steps[i], steps[i], steps[i]);
    // uneven source size larger than window
    test_xdelta3_encoding(dsteps[i] << 1, modCount, steps[i], steps[i], steps[i]);
    // uneven buffer
    test_xdelta3_encoding(steps[i], modCount, dsteps[i], steps[i], steps[i]);
    // uneven block size
    test_xdelta3_encoding(steps[i], modCount, steps[i], steps[i], dsteps[i]);


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
    buffered_source_filter<xdelta3_encoder> filter(&input, &source, MB1, MB1, MB1);
    
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


