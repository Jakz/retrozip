#include "test_support.h"

#include "filters/xdelta3_filter.h"

#if defined(TEST)
#include "catch.h"
#else
#include <cassert>
#define REQUIRE assert
#endif

void testing::createDummyFile(const path& path)
{
  FILE* file = fopen(path.c_str(), "wb+");
  assert(file);
  fclose(file);
}

u32 testing::random(u32 modulo)
{
  static std::random_device device;
  static std::default_random_engine engine(device());
  return engine() % modulo;
}

memory_buffer testing::randomStackDataSource(size_t size)
{
  memory_buffer buffer(size);
  for (size_t i = 0; i < size; ++i)
    buffer.raw()[i] = rand()%256;
  buffer.advance(size);
  return buffer;
}

memory_buffer* testing::randomDataSource(size_t size)
{
  memory_buffer* buffer = new memory_buffer(size);
  for (size_t i = 0; i < size; ++i)
    buffer->raw()[i] = rand()%256;
  buffer->advance(size);
  return buffer;
}

memory_buffer* testing::randomCompressibleDataSource(size_t size)
{
  memory_buffer* buffer = new memory_buffer(size);
  for (size_t i = 0; i < size; ++i)
    buffer->raw()[i] = i / (size/256);
  buffer->advance(size);
  return buffer;
}


void testing::ArchiveTester::release(const ArchiveFactory::Data& data)
{
  for (const auto& entry : data.entries)
  {
    //for (const filter_builder* filter : entry.filters)
    //  delete filter;
    delete entry.source;
  }
  
  //for (const auto& stream : data.streams)
  //  for (const filter_builder* filter : stream.filters)
  //    delete filter;
}

void testing::ArchiveTester::verifyFilters(const std::vector<filter_builder*>& original, const filter_builder_queue& match)
{
  for (size_t j = 0; j < original.size(); ++j)
  {
    const auto& dfilter = original[j];
    const auto& filter = match[j];
    
    REQUIRE(dfilter->identifier() == filter->identifier());
    REQUIRE(dfilter->payload() == filter->payload());
  }
}

void testing::ArchiveTester::verify(const ArchiveFactory::Data& data, const Archive& verify, memory_buffer& buffer)
{
  /* amount of entries / streams */
  REQUIRE(data.entries.size() == verify.entries().size());
  REQUIRE(data.streams.size() == verify.streams().size());
  
  /* basic entry data */
  for (size_t i = 0; i < data.entries.size(); ++i)
  {
    const auto& dentry = data.entries[i];
    const auto& entry = verify.entries()[i];
    
    REQUIRE(dentry.name == entry.name()); /* name match */
    REQUIRE(((memory_buffer*)dentry.source)->size() == entry.binary().originalSize); /* uncompressed size match */
    
    REQUIRE(dentry.filters.size() == entry.filters().size()); /* filter count match */
    
    /* each filter must match */
    verifyFilters(dentry.filters, entry.filters());
  }
  
  for (size_t i = 0; i < data.streams.size(); ++i)
  {
    const auto& dstream = data.streams[i];
    const auto& stream = verify.streams()[i];
    
    REQUIRE(dstream.filters.size() == stream.filters().size()); /* filter count match */
    
    /* entries match */
    REQUIRE(dstream.entries.size() == stream.entries().size());
    REQUIRE(dstream.entries == stream.entries());
    
    /* correct mapping of entries */
    for (size_t j = 0; j < dstream.entries.size(); ++j)
    {
      const auto& entry = verify.entries()[dstream.entries[j]];
      
      REQUIRE(entry.binary().stream == i);
      REQUIRE(entry.binary().indexInStream == j);
    }
    
    //TODO: this is only true for seekable streams, for now this simplified check is done
    /* size of stream == sum of compressed size of entries */
    //if (stream.entries().size() == 1)
    //  REQUIRE(stream.binary().length == verify.entries()[0].binary().compressedSize);
    
    /*REQUIRE(stream.binary().length == std::accumulate(stream.entries().begin(), stream.entries().end(), 0UL, [&verify] (size_t length, ArchiveEntry::ref index) {
     const auto& entry = verify.entries()[index];
     return length + entry.binary().compressedSize;
     }));*/
    
    /* each filter must match */
    verifyFilters(dstream.filters, stream.filters());
  }
  
  const size_t payloadSizeForEntries = std::accumulate(verify.entries().begin(), verify.entries().end(), 0UL, [] (size_t count, const ArchiveEntry& entry) {
    return count + entry.binary().payloadLength;
  });
  
  const size_t payloadSizeForStream = std::accumulate(verify.streams().begin(), verify.streams().end(), 0UL, [] (size_t count, const ArchiveStream& stream) {
    return count + stream.binary().payloadLength;
  });
  
  if (payloadSizeForStream > 0)
  {
    REQUIRE(verify.section(box::Section::STREAM_PAYLOAD));
    REQUIRE(verify.section(box::Section::STREAM_PAYLOAD)->size == payloadSizeForStream);
  }
  
  /* size of archive must match, header + entry*entries + stream*streams + entry names */
  size_t archiveSize = sizeof(box::Header)
  + sizeof(box::Entry) * data.entries.size()
  + sizeof(box::Stream) * data.streams.size()
  + ((!data.entries.empty() && !data.streams.empty()) ? sizeof(box::SectionHeader)*4 : 0) /* entry table, stream table, stream data, entry names section headers */
  + (payloadSizeForEntries > 0 ? sizeof(box::SectionHeader) : 0)
  + (payloadSizeForStream > 0 ? sizeof(box::SectionHeader) : 0)
  + std::accumulate(verify.streams().begin(), verify.streams().end(), 0UL, [] (size_t count, const ArchiveStream& entry) { return entry.binary().length + count; })
  + std::accumulate(data.entries.begin(), data.entries.end(), 0UL, [] (size_t count, const ArchiveFactory::Entry& entry) { return entry.name.length() + 1 + count; })
  + payloadSizeForEntries + payloadSizeForStream;
  
  REQUIRE(buffer.size() == archiveSize);
  
  /* now we need to verify stream data */
  for (size_t i = 0; i < data.entries.size(); ++i)
  {
    const auto& entry = verify.entries()[i];
    ArchiveReadHandle handle(buffer, verify, entry);
    
    data_source* source = handle.source(true);
    
    memory_buffer sink;
    passthrough_pipe pipe(source, &sink, entry.binary().originalSize);
    pipe.process();
    
    REQUIRE(*((memory_buffer*)data.entries[i].source) == sink);
    
    hash::crc32_digester crc32;
    hash::md5_digester md5;
    hash::sha1_digester sha1;
    
    crc32.update(sink.raw(), sink.size());
    md5.update(sink.raw(), sink.size());
    sha1.update(sink.raw(), sink.size());
    
    REQUIRE(entry.binary().digest.crc32 == crc32.get());
    REQUIRE(entry.binary().digest.md5 == md5.get());
    REQUIRE(entry.binary().digest.sha1 == sha1.get());
  }
}

#pragma mark xdelta3 test support

void testing::Xdelta3Tester::test(size_t testLength, size_t modificationCount, size_t bufferSize, size_t windowSize, size_t blockSize)
{
  assert(windowSize >= KB16 && windowSize <= MB16);
  
  if (useRealTool)
  {
    /*remove("source.bin");
     remove("input.bin");
     remove("generated.bin");
     remove("patch.xdelta3");*/
  }
  
  byte* bsource = new byte[testLength];
  for (size_t i = 0; i < testLength; ++i) bsource[i] = rand()%256;
  
  byte* binput = new byte[testLength];
  memcpy(binput, bsource, testLength);
  for (size_t i = 0; i < modificationCount; ++i) binput[rand()%(testLength)] = rand()%256;
  
  memory_buffer source(bsource, testLength);
  memory_buffer input(binput, testLength);
  
  memory_buffer sink(testLength >> 1);
  memory_buffer generated(testLength);
  
  if (useSinkFilters)
  {
    sink_filter<compression::deflater_filter> deflater(&sink, bufferSize);
    sink_filter<xdelta3_encoder> encoder(useDeflater ? &deflater : (data_sink*)&sink, &source, bufferSize, windowSize, blockSize);
    
    passthrough_pipe pipe(&input, &encoder, windowSize);
    pipe.process();
  }
  else
  {
    source_filter<xdelta3_encoder> encoder(&input, &source, bufferSize, windowSize, blockSize);
    source_filter<compression::deflater_filter> deflater(&encoder, bufferSize);
    
    passthrough_pipe pipe(useDeflater ? &deflater : (data_source*)&encoder, &sink, windowSize);
    pipe.process();
  }
  
  if (useRealTool)
  {
    source.rewind();
    input.rewind();
    
    source.serialize(file_handle("source.bin", file_mode::WRITING));
    input.serialize(file_handle("input.bin", file_mode::WRITING));
    
    sink.serialize(file_handle("patch.xdelta3", file_mode::WRITING));
    
    assert(system("/usr/local/bin/xdelta3 -f -d -s source.bin patch.xdelta3 generated.bin") == 0);
    generated.unserialize(file_handle("generated.bin", file_mode::READING));
  }
  else if (useSinkFilters)
  {
    sink.rewind();
    
    sink_filter<xdelta3_decoder> decoder(&generated, &source, bufferSize, windowSize, blockSize);
    sink_filter<compression::inflater_filter> inflater(&decoder, bufferSize);
    
    passthrough_pipe pipe(&sink, useDeflater ? &inflater : (data_sink*)&decoder, windowSize);
    pipe.process();
  }
  else
  {
    sink.rewind();
    
    source_filter<compression::inflater_filter> inflater(&sink, bufferSize);
    source_filter<xdelta3_decoder> decoder(useDeflater ? (data_source*)&inflater : &sink, &source, bufferSize, windowSize, blockSize);
    passthrough_pipe pipe(&decoder, &generated, windowSize);
    pipe.process();
  }
  
  
  REQUIRE(generated == input);
  bool success = generated == input;
  
  out << "Test " << (success ? "success" : "failed") << " ";
  out << " (source: " << strings::humanReadableSize(testLength, false) << ", mods: ";
  out << modificationCount << ", winsize: ";
  out << strings::humanReadableSize(windowSize, false) << ", blksize: ";
  out << strings::humanReadableSize(blockSize, false) << ", bufsize: " ;
  out << strings::humanReadableSize(bufferSize, false) << ")";
  
  if (success)
    out << std::endl;
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
    
    out << " difference in range [" << min << ", " << max << "]" << std::endl;
  }
}
