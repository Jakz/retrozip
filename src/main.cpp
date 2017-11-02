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
  xd3_stream _stream;
  xd3_config _config;
  xd3_source _source;

  xd3_function FUNCTION = xd3_encode_input;
  
  static constexpr usize_t DEFAULT_WIN_SIZE = 4096;
public:
  void init() override;
  void process() override;
  void finalize() override;
};

void xdelta3_filter::init()
{

}

void xdelta3_filter::process()
{
  
}

void xdelta3_filter::finalize()
{
  
}



int main(int argc, const char * argv[])
{
  constexpr size_t SIZE = 1 << 20;
  constexpr size_t INPUT_SIZE = 1 << 20;
  
  byte* source = new byte[SIZE];
  for (size_t i = 0; i < SIZE; ++i) source[i] = rand()%256;
  
  byte* input = new byte[SIZE];
  for (size_t i = 0; i < 1024; ++i) input[rand()%(SIZE)] = rand()%256;
  
  byte* output = new byte[512];
  memset(output, 0, 512);
  usize_t outputSize;
  
  xd3_stream stream;
  xd3_config config;
  xd3_source src;
  
  memset(&src, 0, sizeof(src));
  memset(&stream, 0, sizeof(stream));
  memset(&config, 0, sizeof(config));
  
  if (source != NULL)
  {
    //src.size = SIZE;
    src.blksize = SIZE;
    src.curblkno = 0;
    src.onblk = SIZE;
    src.curblk = source;
    src.max_winsize = SIZE;
    xd3_set_source(&stream, &src);
  }
  
  //config.flags = flags;
  config.winsize = INPUT_SIZE;
  
  //... set smatcher, appheader, encoding-table, compression-level, etc.
  
  /*
   (xd3_stream    *stream,
   const uint8_t *input,
   usize_t         input_size,
   uint8_t       *output,
   usize_t        *output_size,
   usize_t         output_size_max)
   */
  
  xd3_config_stream(&stream, &config);
  int r = xd3_encode_stream(&stream, input, INPUT_SIZE, output, &outputSize, 512);
  printf("%s %s\n", nameForXdeltaReturnValue(r), stream.msg);
  xd3_free_stream(&stream);
}


