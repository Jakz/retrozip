#include "filter_queue.h"

const filter_repository* filter_repository::instance()
{
  static bool init = false;
  static filter_repository repository;
  
  if (!init)
  {
    repository.registerGenerator(builders::identifier::XOR_FILTER, [] (const byte* payload) {
      return [payload] (const size_t bufferSize)
      {
        return new builders::xor_builder(bufferSize, payload);
      };
    });
    
    
    init = true;
  }
  
  return &repository;
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

      if (header->length < data.toRead() + sizeof(box::Payload))
        throw exceptions::unserialization_exception("error in payload, data is not long enough"); //TODO improve

      hasNext = header->hasNext;
      
      box::payload_uid identifier = header->identifier;
      const byte* rawData = data.direct() + sizeof(box::Payload);
      
      add(filter_repository::instance()->generate(identifier, rawData));
    }
  }
}
