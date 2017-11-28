#include "archive_builder.h"

filter_builder* ArchiveBuilder::buildLZMA()
{
  return new builders::lzma_builder(_bufferSize);
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
    return std::unique_ptr<seekable_data_source>(source);
  });
  
  return sources;
}
