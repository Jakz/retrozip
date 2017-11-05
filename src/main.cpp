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

class xdelta3_filter : public buffered_filter
{
private:
  seekable_data_source* _source;
  memory_buffer _sourceBuffer;
  
  xd3_stream _stream;
  xd3_config _config;
  xd3_source _xsource;
  int _state;

  xd3_function FUNCTION = xd3_encode_input;
  
  const usize_t _windowSize;
  const usize_t _sourceBlockSize;
  
public:
  xdelta3_filter(seekable_data_source* source, size_t bufferSize, usize_t xdeltaWindowSize, usize_t sourceBlockSize) :
    buffered_filter(bufferSize, bufferSize), _source(source),
    _windowSize(xdeltaWindowSize), _sourceBuffer(sourceBlockSize), _sourceBlockSize(sourceBlockSize) { }
  
  void init() override;
  void process() override;
  void finalize() override;
};

void xdelta3_filter::init()
{
  memset(&_stream, 0, sizeof(_stream));
  memset(&_config, 0, sizeof(_config));
  memset(&_xsource, 0, sizeof(_xsource));

  /* configuration */
  xd3_init_config(&_config, 0);
  
  _config.winsize = _windowSize;
  _config.sprevsz = _windowSize >> 2;
  _config.getblk = nullptr;
  
  _config.flags = XD3_SEC_DJW | XD3_COMPLEVEL_9;
  
  int r = xd3_config_stream(&_stream, &_config);
  assert(r == 0);
  
  
  /* setting source */
  _xsource.blksize = _sourceBlockSize;
  _xsource.max_winsize = _sourceBlockSize;
  
  _xsource.onblk = 0;
  _xsource.curblkno = 0;
  _xsource.curblk = nullptr;
  
  r = xd3_set_source(&_stream, &_xsource);
  assert(r == 0);
  
  _state = XD3_INPUT;
}

void xdelta3_filter::process()
{
  if (_isEnded && !(_stream.flags & XD3_FLUSH))
  {
    //printf("%p: xdelta3::process flush request (setting XD3_FLUSH flag)\n", this);
    xd3_set_flags(&_stream, _stream.flags | XD3_FLUSH);
  }
  
  usize_t effective = xd3_min(_stream.winsize, _in.used());
  
  if (_state == XD3_INPUT && effective > 0)
  {
    xd3_avail_input(&_stream, _in.head(), effective);
    
    static size_t ___total_in = 0;
    ___total_in += _in.used();
    printf("%p: xdelta3::process consumed %lu bytes (XD3_INPUT) (total: %lu)\n", this, _in.used(), ___total_in);
    
    _in.consume(_in.used());
  }



  /*_source.curblkno = _stream.total_in / _source.blksize;
  _source.onblk = _stream.total_in % _source.blksize;
  _source.curblk =  _in.head();*/
  
  _state = xd3_encode_input(&_stream);
  
  //printf(">> %s %s\n", nameForXdeltaReturnValue(r), _stream.msg);

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
    {
      break;
    }
      
    case XD3_GOTHEADER:
    {
      //printf("%p: xdelta3::process got %s\n", this, nameForXdeltaReturnValue(_state));
      break;
    }
      
    case XD3_WINSTART:
    {
      //printf("%p: xdelta3::process started window (avail_in: %lu) (XD3_WINSTART)\n", this, _stream.avail_in);
      break;
    }
      
    case XD3_WINFINISH:
    {
      //printf("%p: xdelta3::process finished window (XD3_WINFINISH)\n", this);
      
      if (_isEnded && _stream.buf_avail == 0 && _stream.buf_leftover == 0)
      {
        //printf("%p: xdelta::process input was finished so encoding is finished\n", this);
        _finished = true;
      }
      
      break;
    }
      
    case XD3_GETSRCBLK:
    {
      //printf("%p: xdelta3::process block request %lu (XD3_GETSRCBLK)\n", this, _xsource.getblkno);
      
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
      
    case 0:
    {
      break;
    }
  }
}

void xdelta3_filter::finalize()
{
  xd3_close_stream(&_stream);
  xd3_free_stream(&_stream);
}



int main(int argc, const char * argv[])
{
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
    buffered_source_filter<xdelta3_filter> filter(&input, &source, MB1, MB1, MB1);
    
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

    sink.serialize(file_handle("patch-buggy.xdelta3", file_mode::WRITING));
  }
  
  /*{
    memory_buffer sink;
    buffered_source_filter<xdelta3_filter> filter(&input, &source, 1 << 21);
    
    source.rewind();
    input.rewind();
    
    passthrough_pipe pipe(&filter, &sink, 256);
    pipe.process();

    sink.serialize(file_handle("patch.xdelta3", file_mode::WRITING));
  }*/
  
  return 0;
}


