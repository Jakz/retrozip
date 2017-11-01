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

#define REQUIRE assert

int main(int argc, const char * argv[])
{
  constexpr size_t LEN = 1 << 18;
  
  byte* testData = new byte[LEN];
  for (size_t i = 0; i < LEN; ++i)
    testData[i] = (i/8) % 256;
  
  memory_buffer source(testData, LEN);
  delete [] testData;
  
  buffered_source_filter<compression::deflater_filter> deflater(&source, 1024);
  buffered_source_filter<compression::inflater_filter> inflater(&deflater, 512);
  
  memory_buffer sink;
  
  passthrough_pipe pipe(&inflater, &sink, 1024);
  pipe.process();
  
  REQUIRE(deflater.zstream().total_in == source.size());
  REQUIRE(deflater.zstream().total_out == inflater.zstream().total_in);
  REQUIRE(inflater.zstream().total_out == source.size());
  
  REQUIRE(source == sink);
}
