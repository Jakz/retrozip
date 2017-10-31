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

int main(int argc, const char * argv[])
{
  log_data_source source(1024, 32);
  log_data_sink sink(16);
  
  passthrough_pipe pipe(&source, &sink, 10);
  pipe.process();
}
