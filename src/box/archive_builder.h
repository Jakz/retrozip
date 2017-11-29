#pragma once

#include "base/path.h"

#include "core/file_data_source.h"

#include "archive.h"

struct named_seekable_source
{
  std::string name;
  std::unique_ptr<seekable_data_source> source;
  
  seekable_data_source* operator->() const { return source.get(); }
  operator seekable_data_source*() const { return source.get(); }
  
  named_seekable_source(std::string&& name, seekable_data_source* source) :
  name(name), source(source) { }
};

using data_source_vector = std::vector<named_seekable_source>;
using path_vector = std::vector<path>;
using entry_name_builder = std::function<std::string(const path& path)>;

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
  
  CompressionPolicy() : CompressionPolicy(Mode::LZMA, 9, true) { }
  CompressionPolicy(Mode mode, Level level, bool extreme)
    : mode(mode), level(level), extreme(extreme) { }
};

struct RamUsagePolicy
{
  
};

class ArchiveBuilder
{
private:
  CompressionPolicy _compressionPolicy;
  CachePolicy _sourceCachingPolicy;
  entry_name_builder _entryNameBuilder;
  size_t _bufferSize;
  
  filter_builder* buildLZMA();
  filter_builder* buildDeflater();

public:
  ArchiveBuilder(CachePolicy sourceCachingPolicy)
  : _sourceCachingPolicy(sourceCachingPolicy), _entryNameBuilder([](const path& path) { return path.filename(); }),
  _bufferSize(MB1) { }
  
  size_t maxBufferSize(const data_source_vector& sources);
  size_t bufferSizeForPolicy(const data_source_vector& sources);
  
  data_source_vector buildSources(const path_vector& paths);
  data_source_vector buildSourcesFromFolder(const path& path);
  
  filter_builder* buildDefaultCompressor();
  
  Archive buildBestSingleStreamDeltaArchive(const data_source_vector& sources);
  Archive buildSingleStreamBaseWithDeltasArchive(const data_source_vector& sources, size_t baseIndex);
  Archive buildSingleStreamSolidArchive(const data_source_vector& sources);
  
  
  void extractWholeArchiveIntoFolder(const path& archive, const path& destination);
};
