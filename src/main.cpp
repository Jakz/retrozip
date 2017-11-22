//
//  main.cpp
//  retrozip
//
//  Created by Jack on 26/10/17.
//  Copyright © 2017 Jack. All rights reserved.
//

#include <iostream>
#include "box/header.h"
#include "box/archive.h"
#include "filters/filters.h"
#include "core/data_source.h"
#include "core/file_data_source.h"
#include "filters/deflate_filter.h"
#include "filters/lzma_filter.h"
#include "filters/xdelta3_filter.h"

#define REQUIRE assert
#define WRITE_RANDOM_DATA_AND_REWIND(dest, name, length) byte name[(length)]; randomize(name, (length)); (dest).write(name, 1, (length)); (dest).rewind();
#define WRITE_RANDOM_DATA(dest, name, length) byte name[(length)]; randomize(name, (length)); dest.write(name, 1, (length));
void randomize(byte* data, size_t len) { for (size_t i = 0; i < len; ++i) { data[i] = rand()%256; } }
#define READ_DATA(dest, name, length, res) byte name[(length)]; size_t res = dest.read(name, 1, (length));

#include <sstream>

void test_xdelta3_huge()
{
  path sourcePath = path("/Volumes/RAMDisk/pes_it.iso");
  path inputPath = path("/Volumes/RAMDisk/pes_fr.iso");
  
  file_data_source source(sourcePath);
  file_data_source input(inputPath);
  
  
  unbuffered_source_filter<lambda_unbuffered_data_filter> inputCounter(&input, [] (const byte*, size_t amount, size_t effective) {
    const static size_t modulo = MB1;
    static size_t current = 0;
    static size_t steps = 0;
    
    current += effective;
    
    while (current > modulo)
    {
      ++steps;
      current -= modulo;
    }
    
    printf("Processed %s bytes\n", strings::humanReadableSize(steps*modulo, true).c_str());
  });
  
  source_filter<xdelta3_encoder> encoder(&inputCounter, &source, MB128, MB16, GB2);
  
  
  memory_buffer sink;
  
  passthrough_pipe pipe(&encoder, &sink, MB16);
  pipe.process();
  
  sink.serialize(file_handle("/Volumes/RAMDisk/patch-rzip.xdelta3", file_mode::WRITING));
}

static std::stringstream ss;
static std::stringstream sc;
void test_xdelta3_encoding(size_t testLength, size_t modificationCount, size_t bufferSize, size_t windowSize, size_t blockSize)
{
  constexpr bool TEST_WITH_REAL_TOOL = false;
  constexpr bool USE_DEFLATER = true;
  constexpr bool USE_SINK_FILTERS = true;
  
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
  
  if (USE_SINK_FILTERS)
  {
    sink_filter<compression::deflater_filter> deflater(&sink, bufferSize);
    sink_filter<xdelta3_encoder> encoder(USE_DEFLATER ? &deflater : (data_sink*)&sink, &source, bufferSize, windowSize, blockSize);
    
    passthrough_pipe pipe(&input, &encoder, windowSize);
    pipe.process();
  }
  else
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
  else if (USE_SINK_FILTERS)
  {
    sink.rewind();
    
    sink_filter<xdelta3_decoder> decoder(&generated, &source, bufferSize, windowSize, blockSize);
    sink_filter<compression::inflater_filter> inflater(&decoder, bufferSize);
    
    passthrough_pipe pipe(&sink, USE_DEFLATER ? &inflater : (data_sink*)&decoder, windowSize);
    pipe.process();
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


int mainzzz(int argc, const char * argv[])
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

#include <numeric>

int main(int argc, const char * argv[])
{
  memory_buffer source(MB1);
  for (size_t i = 0; i < MB1; ++i)
    source.raw()[i] = (i / 50) % 256;
  source.advance(MB1);
    
  memory_buffer sink;
  
  source_filter<compression::lzma_encoder> filter(&source, 1024);
  source_filter<compression::lzma_decoder> filter2(&filter, 256);
  
  passthrough_pipe pipe(&filter2, &sink, 1024);
  pipe.process();
  
  REQUIRE(sink == source);
  
  return 0;
}
