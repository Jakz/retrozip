#pragma once

#include "tbx/base/path.h"

#include "tbx/streams/file_data_source.h"

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

struct BufferSizePolicy
{
  enum class Mode
  {
    FIXED,
    AS_LARGE_AS_SOURCE
  };
  
  Mode mode;
  size_t size;
  
  BufferSizePolicy(Mode mode) : mode(mode), size(MB32) { }
  BufferSizePolicy(Mode mode, size_t size) : mode(mode), size(size) { }
  BufferSizePolicy(size_t size) : BufferSizePolicy(Mode::FIXED, size) { }
  
  operator size_t() const
  {
    assert(mode == Mode::FIXED);
    return size;
  }
};

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
  BufferSizePolicy _filterBufferPolicy;
  BufferSizePolicy _pipeBufferPolicy;
  CompressionPolicy _compressionPolicy;
  CachePolicy _sourceCachingPolicy;
  entry_name_builder _entryNameBuilder;
  
  filter_builder* buildLZMA(const data_source_vector& sources);
  filter_builder* buildDeflater(const data_source_vector& sources);
  
  enum class Log { LOG_INFO, LOG_ERROR };
  
  template<typename... Args> void log(Log log, const std::string& format, Args... args);
  template<typename... Args> void error(const std::string& format, Args... args) { log(Log::LOG_ERROR, format, args...); }

  
public:
  ArchiveBuilder(CachePolicy sourceCachingPolicy, BufferSizePolicy filterBufferPolicy, BufferSizePolicy pipeBufferPolicy)
  : _sourceCachingPolicy(sourceCachingPolicy), _filterBufferPolicy(filterBufferPolicy), _pipeBufferPolicy(pipeBufferPolicy),
  _entryNameBuilder([](const path& path) { return path.filename(); })
  { }
  
  size_t maxBufferSize(const data_source_vector& sources);
  size_t filterBufferSizeForPolicy(const data_source_vector& sources);
  
  data_source_vector buildSources(const path_vector& paths);
  data_source_vector buildSourcesFromFolder(const path& path);
  
  filter_builder* buildDefaultCompressor(const data_source_vector& sources);
  
  Archive buildBestSingleStreamDeltaArchive(const data_source_vector& sources);
  Archive buildSingleStreamBaseWithDeltasArchive(const data_source_vector& sources, size_t baseIndex);
  Archive buildSingleStreamSolidArchive(const data_source_vector& sources);
  Archive buildSolidArchivePerFolderOfDirectoryTree(const path& root);
  
  
  void extractWholeArchiveIntoFolder(const class path& path, const class path& destination);
};
