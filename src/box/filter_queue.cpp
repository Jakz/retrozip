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
