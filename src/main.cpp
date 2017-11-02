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
  constexpr size_t LEN = 1024;
  
  memory_buffer source;
  byte* data = new byte[LEN];
  for (size_t i = 0; i < LEN; ++i)
    data[i] = 0x88;
  
  source.write(data, LEN);
  source.rewind();
  
  sink_factory factory = []() { return new memory_buffer(); };
  multiple_fixed_size_sink_policy policy(factory, { 256LL, 256LL, 512LL });
  multiple_data_sink sink(&policy);
  
  passthrough_pipe pipe(&source, &sink, 100);
  pipe.process();
}
