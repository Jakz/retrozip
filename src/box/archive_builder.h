#pragma once

#include "base/path.h"

#include "core/file_data_source.h"

#include "archive.h"

using data_source_vector = std::vector<std::unique_ptr<seekable_data_source>>;
using path_vector = std::vector<path>;

struct CachePolicy
{
  enum class Mode
  {
    ALWAYS,
    NEVER,
    LESS_THAN_THRESHOLD
  };
  
  Mode mode;
  size_t threshold;
  
  CachePolicy(Mode mode, size_t threshold)
    : mode(mode), threshold(threshold) { }
};

struct CompressionPolicy
{
  enum class Mode
  {
    UNCOMPRESSED,
    DEFLATE,
    LZMA
  };
  
  using Level = u32;
  
  Mode mode;
  Level level;
  bool extreme;
  
  CompressionPolicy(Mode mode, Level level, bool extreme)
    : mode(mode), level(level), extreme(extreme) { }
  
};

class ArchiveBuilder
{
private:
  CachePolicy _sourceCachingPolicy;
  size_t _bufferSize;
  
  filter_builder* buildLZMA();

public:
  ArchiveBuilder(CachePolicy sourceCachingPolicy) : _sourceCachingPolicy(sourceCachingPolicy), _bufferSize(MB1) { }
  
  data_source_vector buildSources(const path_vector& paths);
};
