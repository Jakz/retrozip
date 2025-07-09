#include "filter_queue.h"

#include "archive.h" /* needed for options() */

const Options& archive_environment::options() const { return archive->options(); }

#include <sstream>

const filter_repository* filter_repository::instance()
{
  static bool init = false;
  static filter_repository repository;
  
  if (!init)
  {
    repository.registerGenerator(builders::identifier::XOR_FILTER, [] (const byte* payload, const archive_environment& env) {
      size_t bufferSize = env.options().bufferSize;
      return new builders::xor_builder(bufferSize, payload);
    });
    
    repository.registerGenerator(builders::identifier::DEFLATE_FILTER, [] (const byte* payload, const archive_environment& env) {
      size_t bufferSize = env.options().bufferSize;
      return new builders::deflate_builder(bufferSize);
    });
    
    repository.registerGenerator(builders::identifier::LZMA_FILTER, [] (const byte* payload, const archive_environment& env) {
      size_t bufferSize = env.options().bufferSize;
      return new builders::lzma_builder(bufferSize);
    });
    
    repository.registerGenerator(builders::identifier::XDELTA3_FILTER, [] (const byte* payload, const archive_environment& env) {
      size_t bufferSize = env.options().bufferSize;
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
    throw exceptions::unserialization_exception(fmt::format("unknown filter identifier: {}", identifier));
  else
    return it->second(data, env);
}


std::string filter_builder_queue::mnemonic(bool shortMode) const
{
  std::stringstream ss;
  
  const char* separator = ";";
  const char* sep = "";
  
  for (const auto& builder : _builders)
  {
    ss << builder->mnemonic(shortMode) << sep;
    sep = separator;
  }
  
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
      /* read header */
      const box::Payload* header = reinterpret_cast<const box::Payload*>(data.direct());

      /* if we expect more data which is not available something is wrong */
      if (header->length > data.toRead() + sizeof(box::Payload))
        throw exceptions::unserialization_exception("error in payload, data is not long enough"); //TODO improve

      hasNext = header->hasNext;
      
      /* create filter according to payload and identifier */
      box::payload_uid identifier = header->identifier;
      add(filter_repository::instance()->generate(identifier, data.direct(), env));

      /* seek to next payload*/
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
  return new source_filter<xdelta3_decoder>(source, _sourceWrapper.get(), _bufferSize, _xdeltaWindowSize, _sourceBlockSize);
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
    
    auto source = _source;
    
    if (env.options().isMultithreaded())
    {
      seekable_source_slice slice(_source);
      source = &slice;
    }
    
    unbuffered_source_filter<filters::data_counter> counter(source);
    unbuffered_source_filter<filters::multiple_digest_filter> digester(&counter);
    null_data_sink sink;
    passthrough_pipe pipe(&digester, &sink, _bufferSize);
    pipe.process();
    this->_sourceDigest = box::DigestInfo(counter.filter().count(), digester.filter().crc32(), digester.filter().md5(), digester.filter().sha1());
    env.digestCache.emplace(std::make_pair(_source, _sourceDigest));
  }
}

class xdelta3_prepare_task : public process_task
{
protected:
  const archive_environment* _env;
  const ArchiveEntry* _entry;
  ArchiveReadHandle* _handle;

  builders::xdelta3_builder* _builder;
  data_source* _source;
  memory_buffer* _sink;

public:
  xdelta3_prepare_task(builders::xdelta3_builder* builder, const archive_environment* env, const ArchiveEntry* entry) 
  : process_task("xdelta3-base-extract"),
    _env(env),
    _entry(entry),
    _handle(nullptr),
    _builder(builder),
    _source(nullptr),
    _sink(nullptr)
  {

  }

  ~xdelta3_prepare_task()
  {
    delete _handle;
    delete _sink;
  }

  data_source* source() const override { return _source; }
  data_sink* sink() const override { return _sink; }
  virtual size_t size() const override { return _sink->capacity(); }

  void prepare() override
  {
    /* prepare a whole buffer for the uncompressed entry which is the source of xdelta3 diff */
    memory_buffer* sink = new memory_buffer(_entry->binary().digest.size);
    _handle = new ArchiveReadHandle(*_env->r, *_env->archive, *_entry);

    _source = _handle->source(true);
    _sink = sink;
  }

  void finalize() override
  {
    //TODO: for now simple solution, just fill the entry after this one in the process task list

    /* cache entry in environment so that successive tasks will be able to use it */
    _env->cache.emplace(std::make_pair(_entry->binary().digest, std::unique_ptr<seekable_data_source>(_sink)));
    /* set source in original xdelta task, we expect this task to be executed before */
    _builder->setSource(_sink);

    auto filter = _handle->filterCache().get();
  }
};


void builders::xdelta3_builder::unsetup(const archive_environment& env)
{
  assert(_source == nullptr);
  
  /* search for matching source between entries */
  size_t i = 0;
  for (const auto& entry : env.archive->entries())
  {
    /* we found a matching source */
    if (entry.binary().digest == _sourceDigest)
    {
      TRACE_A("%p: xdelta3_builder::unsetup() found matching source %s", this, std::string(entry.name()).c_str());

      /* prepare extraction of source task and add it */
      xdelta3_prepare_task* task = new xdelta3_prepare_task(this, &env, &entry);
      env.tasks.add(task);
      return;
    }

    ++i;
  }
  
  throw exceptions::missing_source_file_exception("can't find required source file to rebuild entry");
  
  //TODO: multiple ways to manage this
}
