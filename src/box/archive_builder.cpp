#include "archive_builder.h"

#include "base/file_system.h"

filter_builder* ArchiveBuilder::buildLZMA(const data_source_vector& sources)
{
  return new builders::lzma_builder(filterBufferSizeForPolicy(sources));
}

filter_builder* ArchiveBuilder::buildDeflater(const data_source_vector& sources)
{
  return new builders::deflate_builder(filterBufferSizeForPolicy(sources));
}

data_source_vector ArchiveBuilder::buildSources(const path_vector& paths)
{
  data_source_vector sources;
  std::transform(paths.begin(), paths.end(), std::back_inserter(sources), [this](const class path& path) {
    seekable_data_source* source = nullptr;
    
    if (this->_sourceCachingPolicy.mode == CachePolicy::Mode::NEVER)
      source = new file_data_source(path);
    else
    {
      bool shouldCache = this->_sourceCachingPolicy.mode == CachePolicy::Mode::ALWAYS;
      file_handle handle = file_handle(path, file_mode::READING);
      
      if (!shouldCache)
        shouldCache = handle.length() < _sourceCachingPolicy.threshold;
      
      if (shouldCache)
      {
        memory_buffer* buffer = new memory_buffer(handle.length());
        assert(handle.read(buffer->raw(), 1, handle.length()) == handle.length());
        buffer->advance(handle.length());
        
        source = buffer;
      }
      else
        source = new file_data_source(path, handle);
    }
    
    assert(source);
    return named_seekable_source(path.filename(), source);
  });
  
  return sources;
}

data_source_vector ArchiveBuilder::buildSourcesFromFolder(const path& path)
{
  const auto* fs = FileSystem::i();
  
  return buildSources(fs->contentsOfFolder(path));
}

size_t ArchiveBuilder::maxBufferSize(const data_source_vector& sources)
{
  size_t maxSize = 0;
  for (const auto& source : sources)
    maxSize = std::max(source.source->size(), maxSize);
  return maxSize;
}

size_t ArchiveBuilder::filterBufferSizeForPolicy(const data_source_vector& sources)
{
  switch (_filterBufferPolicy.mode)
  {
    case BufferSizePolicy::Mode::FIXED: return _filterBufferPolicy.size;
    case BufferSizePolicy::Mode::AS_LARGE_AS_SOURCE: return maxBufferSize(sources);
  }
}

filter_builder* ArchiveBuilder::buildDefaultCompressor(const data_source_vector& sources)
{
  return _compressionPolicy.mode == CompressionPolicy::Mode::LZMA ? buildLZMA(sources) : buildDeflater(sources);
}

Archive ArchiveBuilder::buildSingleStreamSolidArchive(const data_source_vector& sources)
{
  size_t bufferSize = filterBufferSizeForPolicy(sources);
  ArchiveFactory::Data data;
  
  for (const auto& source : sources)
  {
    source->rewind();
    data.entries.push_back({ source.name, source, { } });
  }
  
  ArchiveEntry::ref base = 0;
  std::vector<ArchiveEntry::ref> indices(sources.size());
  std::generate_n(indices.begin(), indices.size(), [&base]() { return base++; });
  data.streams.push_back({ indices, { new builders::lzma_builder(bufferSize) } });
  
  return Archive::ofData(data);
}

Archive ArchiveBuilder::buildSingleStreamBaseWithDeltasArchive(const data_source_vector& sources, size_t baseIndex)
{
  size_t bufferSize = filterBufferSizeForPolicy(sources);

  ArchiveFactory::Data data;
  
  for (box::index_t i = 0; i < sources.size(); ++i)
  {
    const auto& source = sources[i];
    
    source->rewind();
    if (i == baseIndex)
    {
      data.entries.push_back({ source.name, source, { new builders::lzma_builder(bufferSize) } });
    }
    else
      data.entries.push_back({ source.name, source, { new builders::xdelta3_builder(bufferSize, sources[baseIndex], MB16, sources[baseIndex]->size()) } });
    
    data.streams.push_back({ { i } });
  }

  return Archive::ofData(data);
}

Archive ArchiveBuilder::buildBestSingleStreamDeltaArchive(const data_source_vector& sources)
{
  TRACE_AB("%p: builder::bestDeltaArchive()", this, sources.size());
  
  //TODO: totally unoptmized for now
  size_t index = 0;
  size_t size = std::numeric_limits<size_t>::max();
  
  for (size_t i = 0; i < sources.size(); ++i)
  {
    Archive archive = buildSingleStreamBaseWithDeltasArchive(sources, i);
    // TODO: only in memory for now
    memory_buffer buffer;
    archive.write(buffer);
    
    if (buffer.size() < size)
    {
      size = buffer.size();
      index = i;
    }
  }
  
  TRACE_AB("%p builder::bestDeltaArchive(): Best base entry is %s, archive size is %s", this, sources[index].name.c_str(), strings::humanReadableSize(size, false).c_str());
  
  return buildSingleStreamBaseWithDeltasArchive(sources, index);
}


void ArchiveBuilder::extractWholeArchiveIntoFolder(const class path& path, const class path& destination)
{
  const auto* fs = FileSystem::i();
  
  if (!fs->existsAsFile(path))
    throw exceptions::file_not_found(path);
  
  Archive archive;
  file_data_source source(path);
  archive.options().bufferSize = MB64;
  archive.read(source);
  
  for (const auto& entry : archive.entries())
  {
    TRACE_AB("%p: builder::extract() extracting entry %s (%s)", this, entry.name().c_str(), entry.filters().mnemonic().c_str());
    auto handle = ArchiveReadHandle(source, archive, entry);
    auto* entrySource = handle.source(true);
    
    class path dest = destination.append(entry.name());
    file_data_sink sink(dest);
    
    passthrough_pipe pipe(entrySource, &sink, _pipeBufferPolicy);
    pipe.process(entry.binary().digest.size);
  }
}
