#pragma once


#include "filters/filters.h"

#include "header.h"
#include <vector>
#include <unordered_map>

struct filter_builder
{
protected:
  size_t _bufferSize;

  filter_builder(size_t bufferSize) : _bufferSize(bufferSize) { }
  
public:
  virtual data_source* apply(data_source* source) const = 0;
  virtual data_source* unapply(data_source* source) const = 0;
  virtual box::payload_uid identifier() const = 0;
  virtual memory_buffer payload() const = 0;
};

struct symmetric_filter_builder : public filter_builder
{
public:
  symmetric_filter_builder(size_t bufferSize) : filter_builder(bufferSize) { }

  data_source* unapply(data_source* source) const override final { return apply(source); }
};

class filter_cache
{
private:
  data_source* _source;
  data_source* _tail;
  std::vector<std::unique_ptr<data_source>> _filters;
  
public:
  filter_cache(data_source* source) : _source(source), _tail(source) { }
  
  void apply(const filter_builder& builder)
  {
    _tail = builder.apply(_tail);
    _filters.push_back(std::unique_ptr<data_source>(_tail));
  }
  
  void unapply(const filter_builder& builder)
  {
    _tail = builder.unapply(_tail);
    _filters.push_back(std::unique_ptr<data_source>(_tail));
  }
  
  data_source* get() { return _tail; }
};

class filter_builder_queue
{
private:
  std::vector<std::unique_ptr<filter_builder>> _builders;
  
public:
  /* construct whole payload for all the filters of the queue */
  memory_buffer payload() const
  {
    memory_buffer total(256);
    
    for (auto it = _builders.begin(); it != _builders.end(); ++it)
    {
      const auto& builder = *it;
      
      memory_buffer current = builder->payload();
      
      box::Payload payload = { builder->identifier(), current.size() + sizeof(box::Payload), it != _builders.end() - 1 };
      
      total.write(payload);
      total.write(current.data(), 1, current.size());
    }
    
    return total;
  }
  
  void add(filter_builder* builder)
  {
    _builders.push_back(std::unique_ptr<filter_builder>(builder));
  }
  
  filter_cache apply(data_source* source) const
  {
    filter_cache cache = filter_cache(source);
    for (const auto& builder : _builders)
      cache.apply(*builder.get());
    
    return cache;
  }
  
  filter_cache unapply(data_source* source) const
  {
    filter_cache cache = filter_cache(source);
    for (const auto& builder : _builders)
      cache.unapply(*builder.get());
    
    return cache;
  }
};

class filter_repository
{
public:
  using generator_t = std::function<std::function<filter_builder*(size_t)>(const byte* payload)>;
  
private:
  std::unordered_map<box::payload_uid, generator_t> repository;
  
public:
  void registerGenerator(box::payload_uid identifier, generator_t generator)
  {
    repository.emplace(std::make_pair(identifier, generator));
    
    repository[identifier] = generator;
  }
};



#include <vector>
namespace builders
{
  enum identifier : box::payload_uid
  {
    XOR_FILTER = 1ULL
  };
  
  class xor_builder : public symmetric_filter_builder
  {
  private:
    std::vector<byte> _key;
    
  public:
    xor_builder(size_t bufferSize, const std::string key) : symmetric_filter_builder(bufferSize)
    {
      _key.resize(key.size(), 0);
      std::copy(key.begin(), key.end(), _key.begin());
    }
    
    xor_builder(size_t bufferSize, const std::vector<byte> key) : symmetric_filter_builder(bufferSize), _key(key) { }
    
    data_source* apply(data_source* source) const override
    {
      return new source_filter<filters::xor_filter>(source, _bufferSize, _key);
    }
    
    box::payload_uid identifier() const override { return identifier::XOR_FILTER; }
    
    virtual memory_buffer payload() const override
    {
      memory_buffer buffer(sizeof(size_t) + _key.size());
      buffer.write(_key.size());
      buffer.write(_key.data(), 1, _key.size());
      return buffer;
    }
  };
}
