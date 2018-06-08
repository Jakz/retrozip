#pragma once

#include "tbx/base/common.h"
#include "tbx/streams/memory_buffer.h"

#include "box/archive.h"

#include <random>

namespace testing
{
  u32 random(u32 modulo);
  
  memory_buffer randomStackDataSource(size_t size);
  
  memory_buffer* randomDataSource(size_t size);
  memory_buffer* randomCompressibleDataSource(size_t size);
  
  void createDummyFile(const path& path);
  
  struct ArchiveTester
  {
    static void release(const ArchiveFactory::Data& data);
    static void verifyFilters(const std::vector<filter_builder*>& original, const filter_builder_queue& match);
    static void verify(const ArchiveFactory::Data& data, const Archive& verify, memory_buffer& buffer);
  };
  
  struct Xdelta3Tester
  {
  public:
    static constexpr size_t steps[] = { KB16, KB32, KB64, KB256, MB1, MB2 };
    static constexpr size_t dsteps[] = { 16000, 32000, 64000, 256000, 1000000, 2000000 };
    static constexpr size_t count = 4;
    
  private:
    const bool useRealTool, useDeflater, useSinkFilters;
    std::ostream& out;
    
  public:
    Xdelta3Tester(std::ostream& out, bool useRealTool, bool useDeflater, bool useSinkFilters) :
    out(out), useRealTool(useRealTool), useDeflater(useDeflater), useSinkFilters(useSinkFilters) { }
    
    void test(size_t testLength, size_t modificationCount, size_t bufferSize, size_t windowSize, size_t blockSize);
  };
}
