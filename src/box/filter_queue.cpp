#include "filter_queue.h"

const filter_repository* filter_repository::instance()
{
  static bool init = false;
  static filter_repository repository;
  
  if (!init)
  {
    repository.registerGenerator(builders::identifier::XOR_FILTER, [] (const byte* payload) {
      size_t bufferSize = repository.bufferSizeFor(builders::identifier::XOR_FILTER, payload);
      return new builders::xor_builder(bufferSize, payload);
    });
    
    repository.registerGenerator(builders::identifier::DEFLATE_FILTER, [] (const byte* payload) {
      size_t bufferSize = repository.bufferSizeFor(builders::identifier::DEFLATE_FILTER, payload);
      return new builders::deflate_builder(bufferSize);
    });
    
    repository.registerGenerator(builders::identifier::LZMA_FILTER, [] (const byte* payload) {
      size_t bufferSize = repository.bufferSizeFor(builders::identifier::LZMA_FILTER, payload);
      return new builders::lzma_builder(bufferSize);
    });
    
    repository.registerGenerator(builders::identifier::XDELTA3_FILTER, [] (const byte* payload) {
      size_t bufferSize = repository.bufferSizeFor(builders::identifier::XDELTA3_FILTER, payload);
      return new builders::lzma_builder(bufferSize);
    });
    
    
    init = true;
  }
  
  return &repository;
}

filter_builder* filter_repository::generate(box::payload_uid identifier, const byte* data) const
{
  const auto it = repository.find(identifier);
  
  if (it == repository.end())
    throw exceptions::unserialization_exception(fmt::sprintf("unknown filter identifier: %lu", identifier));
  else
    return it->second(data);
}



void filter_builder_queue::unserialize(memory_buffer& data)
{
  bool hasNext = data.size() > 0;
  
  data.rewind();
  
  while (hasNext)
  {
    // TODO: here we're using raw access to buffer, using data.read(..) would be safer

    if (data.toRead() < sizeof(box::Payload))
      throw exceptions::unserialization_exception("error in payload, header is not long enough"); //TODO improve
    else
    {
      const box::Payload* header = reinterpret_cast<const box::Payload*>(data.direct());

      if (header->length > data.toRead() + sizeof(box::Payload))
        throw exceptions::unserialization_exception("error in payload, data is not long enough"); //TODO improve

      hasNext = header->hasNext;
      
      box::payload_uid identifier = header->identifier;
      add(filter_repository::instance()->generate(identifier, data.direct()));
      data.seek(header->length, Seek::CUR);
    }
  }
}



#include "filters/xdelta3_filter.h"

data_source* builders::xdelta3_builder::apply(data_source* source) const
{
  return new source_filter<xdelta3_encoder>(source, _source, _bufferSize, _xdeltaWindowSize, _sourceBlockSize);
}

data_source* builders::xdelta3_builder::unapply(data_source* source) const
{
  return new source_filter<xdelta3_decoder>(source, _source, _bufferSize, _xdeltaWindowSize, _sourceBlockSize);
}

void builders::xdelta3_builder::setup(const archive_environment& env)
{
  _source->rewind();
  
  auto cached = env.digestCache.find(_source);
  
  if (cached != env.digestCache.end())
  {
    TRACE_A("%p: xdelta3_builder::setup() using cached source digest information", this);

    this->_sourceDigest = cached->second;
  }
  else
  {
    TRACE_A("%p: xdelta3_builder::setup() caching source digest information", this);
    
    unbuffered_source_filter<filters::data_counter> counter(_source);
    unbuffered_source_filter<filters::multiple_digest_filter> digester(&counter);
    null_data_sink sink;
    passthrough_pipe pipe(&digester, &sink, _bufferSize);
    pipe.process();
    this->_sourceDigest = box::DigestInfo(counter.filter().count(), digester.filter().crc32(), digester.filter().md5(), digester.filter().sha1());
    env.digestCache.emplace(std::make_pair(_source, _sourceDigest));
  }

}
