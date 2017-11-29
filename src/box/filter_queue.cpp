#include "filter_queue.h"

#include "archive.h" /* needed for options() */

#include <sstream>

const filter_repository* filter_repository::instance()
{
  static bool init = false;
  static filter_repository repository;
  
  if (!init)
  {
    repository.registerGenerator(builders::identifier::XOR_FILTER, [] (const byte* payload, const archive_environment& env) {
      size_t bufferSize = env.archive->options().bufferSize;
      return new builders::xor_builder(bufferSize, payload);
    });
    
    repository.registerGenerator(builders::identifier::DEFLATE_FILTER, [] (const byte* payload, const archive_environment& env) {
      size_t bufferSize = env.archive->options().bufferSize;
      return new builders::deflate_builder(bufferSize);
    });
    
    repository.registerGenerator(builders::identifier::LZMA_FILTER, [] (const byte* payload, const archive_environment& env) {
      size_t bufferSize = env.archive->options().bufferSize;
      return new builders::lzma_builder(bufferSize);
    });
    
    repository.registerGenerator(builders::identifier::XDELTA3_FILTER, [] (const byte* payload, const archive_environment& env) {
      size_t bufferSize = env.archive->options().bufferSize;
      return new builders::xdelta3_builder(bufferSize, payload);
    });
    
    
    init = true;
  }
  
  return &repository;
}

filter_builder* filter_repository::generate(box::payload_uid identifier, const byte* data, const archive_environment& env) const
{
  const auto it = repository.find(identifier);
  
  if (it == repository.end())
    throw exceptions::unserialization_exception(fmt::sprintf("unknown filter identifier: %lu", identifier));
  else
    return it->second(data, env);
}


std::string filter_builder_queue::mnemonic() const
{
  std::stringstream ss;
  for (const auto& builder : _builders) ss << builder->mnemonic() << ";";
  return ss.str();
}

void filter_builder_queue::unserialize(const archive_environment& env, memory_buffer& data)
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
      add(filter_repository::instance()->generate(identifier, data.direct(), env));
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

void builders::xdelta3_builder::unsetup(const archive_environment& env)
{
  assert(_source == nullptr);
  
  /* search for matching source between entries */
  for (const auto& entry : env.archive->entries())
  {
    /* we found a matching source */
    if (entry.binary().digest == _sourceDigest)
    {
      TRACE_A("%p: xdelta3_builder::unsetup() found matching source %s", this, entry.name().c_str());
      
      //TODO: multiple choices here, we could build a source which is read together with this one, cache it on memory, cache it on a file etc
      memory_buffer* sink = new memory_buffer(entry.binary().digest.size);
      ArchiveReadHandle handle = ArchiveReadHandle(*env.r, *env.archive, entry);
      
      //TODO: it could be lazy or not, but source not uses seek asynchronously

      passthrough_pipe pipe(handle.source(true), sink, env.archive->options().bufferSize);
      pipe.process();
      
      _source = sink;
      env.cache.emplace(std::make_pair(_sourceDigest, std::unique_ptr<seekable_data_source>(sink)));
      
      return;
    }
  }
  
  throw exceptions::missing_source_file_exception("can't find required source file to rebuild entry");
  
  //TODO: multiple ways to manage this
  
}
