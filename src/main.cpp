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
  
  
  unbuffered_source_filter<lambda_unbuffered_data_filter> inputCounter(&input, "processer", [] (const byte*, size_t amount, size_t effective) {
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

#include "test/test_support.h"

int mainzzzz(int argc, const char * argv[])
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

  std::stringstream out;
  testing::Xdelta3Tester tester(out, false, true, true);
  
  for (size_t i = 0; i < count; ++i)
  {
    size_t m = steps[i] >> 2;
    size_t p = steps[i];
    size_t d = dsteps[i];
    
    // size_t testLength, size_t modificationCount, size_t bufferSize, size_t windowSize, size_t blockSize
    
    // tests with all values equal
    tester.test(p, m, p, p, p);
    // test with doubled source/input
    tester.test(p << 1, m << 1, p, p, p);
    // test with halved buffer size
    tester.test(p, m, p >> 1, p, p);
    // test with doubled buffer size
    tester.test(p, m, p << 1, p, p);
    
    // uneven source size
    tester.test(d, m, p, p, p);
    // uneven halved buffer size
    tester.test(p, m, d >> 1, p, p);
    
    // uneven source size smaller than window
    tester.test(d, m, p, p, p);
    // uneven source size larger than window
    tester.test(d << 1, m, p, p, p);
    // uneven buffer
    tester.test(p, m, d, p, p);
    // uneven block size (will be adjusted to power of two)
    tester.test(p, m, p, p, d);
    // all uneven
    tester.test(d, m, d, std::max(d, KB16), d);

  
  }
  
  //test_xdelta3_encoding(KB64, 0, KB32, KB16, KB32);

  std::cout << out.str();
  
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
#include "box/archive_builder.h"

int main(int argc, const char * argv[])
{
  ArchiveBuilder builder(CachePolicy(CachePolicy::Mode::ALWAYS, 0));
  
  std::vector<path> paths = {
    "/Volumes/RAMDisk/test/Pocket Monsters - Crystal Version (Japan).gbc",
    "/Volumes/RAMDisk/test/Pokemon - Crystal Version (USA, Europe) (Rev A).gbc",
    "/Volumes/RAMDisk/test/Pokemon - Crystal Version (USA, Europe).gbc",
    "/Volumes/RAMDisk/test/Pokemon - Edicion Cristal (Spain).gbc",
    "/Volumes/RAMDisk/test/Pokemon - Kristall-Edition (Germany).gbc",
    "/Volumes/RAMDisk/test/Pokemon - Version Cristal (France).gbc",
    "/Volumes/RAMDisk/test/Pokemon - Versione Cristallo (Italy).gbc",
  };
  
  size_t baseIndex = 2;
  
  const auto sources = builder.buildSources(paths);

  {
    ArchiveFactory::Data data;
    
    for (box::index_t i = 0; i < paths.size(); ++i)
    {
      sources[i]->rewind();
      
      if (i == baseIndex)
      {
        data.entries.push_back({ paths[i].filename(), sources[i].get(), { new builders::lzma_builder(MB128) } });
      }
      else
        data.entries.push_back({ paths[i].filename(), sources[i].get(), { new builders::xdelta3_builder(MB128, sources[baseIndex].get(), MB16, sources[baseIndex]->size())/*, new builders::lzma_builder(derived1.size() >> 2)*/ } });

      data.streams.push_back({ { i } });
    }
    
    memory_buffer sink;
    Archive archive = Archive::ofData(data);
    archive.options().bufferSize = MB128;
    archive.write(sink);
    sink.serialize(file_handle("/Volumes/RAMDisk/test/test-lzma+delta.box", file_mode::WRITING));
  }
  
  {
    ArchiveFactory::Data data;
    
    for (box::index_t i = 0; i < paths.size(); ++i)
    {
      sources[i]->rewind();
      data.entries.push_back({ paths[i].filename(), sources[i].get(), { } });
    }
    
    ArchiveEntry::ref base = 0;
    std::vector<ArchiveEntry::ref> indices(sources.size());
    std::generate_n(indices.begin(), indices.size(), [&base]() { return base++; });
    data.streams.push_back({ indices, { new builders::lzma_builder(MB16) } });
    
    memory_buffer sink;
    Archive archive = Archive::ofData(data);
    archive.options().bufferSize = MB128;
    archive.write(sink);
    sink.serialize(file_handle("/Volumes/RAMDisk/test/test-solid.box", file_mode::WRITING));
  }
  
  /*{
    std::vector<data_source*> solidSources;
    std::transform(sources.begin(), sources.end(), std::back_inserter(solidSources), [](file_data_source& source) {
      source.rewind();
      return &source;
    });
    multiple_data_source source(solidSources);
    
    file_data_sink sink("/Volumes/RAMDisk/test/solid.lzma");
    source_filter<compression::lzma_encoder> encoder(&source, MB128);
    
    passthrough_pipe pipe(&encoder, &sink, MB128);
    pipe.process();
  }*/
  
  return 0;
}
