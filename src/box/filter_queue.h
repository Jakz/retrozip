#pragma once


#include "filters/filters.h"

#include "header.h"
#include <vector>
#include <unordered_map>
#include <numeric>

class Archive;
class filter_repository;
struct archive_environment
{
  Archive* archive;
  union
  {
    seekable_data_source* r;
    memory_buffer* w;
  };
  
  const filter_repository* repository;
  mutable std::unordered_map<data_source*, box::DigestInfo> digestCache;
  mutable std::unordered_map<box::DigestInfo, std::unique_ptr<data_source>, box::DigestInfo::hash> cache;
  
  /*archive_environment& operator=(const archive_environment&) = delete;
  archive_environment(const archive_environment&) = delete;*/
};

struct filter_builder
{
protected:
  size_t _bufferSize;

protected:
  filter_builder(size_t bufferSize) : _bufferSize(bufferSize) { }
  
public:
  virtual ~filter_builder() { }
  
  virtual data_source* apply(data_source* source) const = 0;
  virtual data_source* unapply(data_source* source) const = 0;
  virtual box::payload_uid identifier() const = 0;
  
  virtual memory_buffer payload() const = 0;
  virtual size_t payloadLength() const = 0;
  
  virtual std::string mnemonic() const = 0;
  
  virtual void setup(const archive_environment& env) { }
  virtual void unsetup(const archive_environment& env) { }
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
  filter_cache() : _source(nullptr), _tail(nullptr) { }
  filter_cache(data_source* source) : _source(source), _tail(source) { }
  
  void setSource(data_source* source) { _source = source; _tail = source; }
  
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
  
  void cache(data_source* source)
  {
    _filters.push_back(std::unique_ptr<data_source>(source));
  }
  
  void clear()
  {
    _filters.clear();
  }
  
  data_source* get() { return _tail; }
};

class filter_builder_queue
{
private:
  std::vector<std::unique_ptr<filter_builder>> _builders;
  
public:
  filter_builder_queue() { }
  filter_builder_queue(const std::vector<filter_builder*>& builders)
  {
    for (filter_builder* builder : builders)
      _builders.emplace_back(builder);
  }
  
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
  
  size_t payloadLength() const
  {
    return std::accumulate(_builders.begin(), _builders.end(), 0UL,
                           [] (size_t length, const decltype(_builders)::value_type& builder) {
                             return length + builder->payloadLength() + sizeof(box::Payload);
                           });
  }
  
  void unserialize(const archive_environment& env, memory_buffer& data);
  
  void setup(const archive_environment& env) const
  {
    for (const auto& builder : _builders)
      builder->setup(env);
  }
  
  void unsetup(const archive_environment& env) const
  {
    for (const auto& builder : _builders)
      builder->unsetup(env);
  }
  
  void add(filter_builder* builder)
  {
    _builders.push_back(std::unique_ptr<filter_builder>(builder));
  }
  
  void apply(filter_cache& cache) const
  {
    for (const auto& builder : _builders)
      cache.apply(*builder.get());
  }
  
  void unapply(filter_cache& cache) const
  {
    for (auto it = _builders.rbegin(); it != _builders.rend(); ++it)
      cache.unapply(*(*it).get());
  }
  
  filter_cache apply(data_source* source) const
  {
    filter_cache cache = filter_cache(source);
    apply(cache);
    return cache;
  }
  
  filter_cache unapply(data_source* source) const
  {
    filter_cache cache = filter_cache(source);
    unapply(cache);
    return cache;
  }
  
  std::string mnemonic() const;
  
  const decltype(_builders)::value_type& operator[](size_t index) const { return _builders[index]; }
  size_t size() const { return _builders.size(); }
  bool empty() const { return _builders.empty(); }
};

class filter_repository
{
public:
  using generator_t = std::function<filter_builder*(const byte* payload, const archive_environment&)>;
  
private:
  std::unordered_map<box::payload_uid, generator_t> repository;
    
public:
  void registerGenerator(box::payload_uid identifier, generator_t generator)
  {
    repository.emplace(std::make_pair(identifier, generator));
    repository[identifier] = generator;
  }
  
  filter_builder* generate(box::payload_uid identifier, const byte* data, const archive_environment& env) const;
  
  static const filter_repository* instance();
};

#include <vector>


#include "filters/deflate_filter.h"
#include "filters/lzma_filter.h"

namespace builders
{
  enum identifier : box::payload_uid
  {
    MIS_FILTERS_BASE = 1ULL,
    XOR_FILTER,
    
    COMPRESSION_FILTERS_BASE = 1024ULL,
    DEFLATE_FILTER,
    LZMA_FILTER,
    
    DIFF_FILTERS_BASE = 2048ULL,
    XDELTA3_FILTER
  };
  
  class xor_builder : public symmetric_filter_builder
  {
  private:
    std::vector<byte> _key;
    
  public:
    xor_builder(size_t bufferSize, const byte* payload) : symmetric_filter_builder(bufferSize)
    {
      payload += sizeof(box::Payload);
      box::slength_t keyLength = *reinterpret_cast<const box::slength_t*>(payload);
      payload += sizeof(box::slength_t);
      
      _key.resize(keyLength, 0);
      std::copy(payload, payload + keyLength, _key.begin());
    }
    
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
    std::string mnemonic() const override { return fmt::sprintf("xor:key=%s", strings::fromByteArray(_key)); }
    
    size_t payloadLength() const override { return sizeof(box::slength_t) + _key.size(); }
    memory_buffer payload() const override
    {
      memory_buffer buffer(payloadLength());
      buffer.write((box::slength_t)_key.size());
      buffer.write(_key.data(), 1, _key.size());
      return buffer;
    }
  };
    
  class deflate_builder : public filter_builder
  {
  private:
    
  public:
    deflate_builder(size_t bufferSize) : filter_builder(bufferSize) { }
    
    box::payload_uid identifier() const override { return identifier::DEFLATE_FILTER; }
    std::string mnemonic() const override { return "deflate"; }
    
    size_t payloadLength() const override { return 0; }
    memory_buffer payload() const override { return memory_buffer(0); }
    
    
    data_source* apply(data_source* source) const override
    {
      return new source_filter<compression::deflater_filter>(source, _bufferSize);
    }
    
    data_source* unapply(data_source* source) const override
    {
      return new source_filter<compression::inflater_filter>(source, _bufferSize);
    }
  };
    
  class lzma_builder : public filter_builder
  {
  private:
    
  public:
    lzma_builder(size_t bufferSize) : filter_builder(bufferSize) { }
    
    box::payload_uid identifier() const override { return identifier::LZMA_FILTER; }
    std::string mnemonic() const override { return "lzma"; }
    
    size_t payloadLength() const override { return 0; }
    memory_buffer payload() const override { return memory_buffer(0); }
    
    data_source* apply(data_source* source) const override
    {
      return new source_filter<compression::lzma_encoder>(source, _bufferSize);
    }
    
    data_source* unapply(data_source* source) const override
    {
      return new source_filter<compression::lzma_decoder>(source, _bufferSize);
    }
  };
    
  class xdelta3_builder : public filter_builder
  {
  private:
    seekable_data_source* _source;
    
    box::DigestInfo _sourceDigest;
    
    size_t _xdeltaWindowSize;
    size_t _sourceBlockSize;
    
  public:
    xdelta3_builder(size_t bufferSize, seekable_data_source* source, size_t xdeltaWindowSize, size_t sourceBlockSize) : filter_builder(bufferSize), _source(source), _xdeltaWindowSize(xdeltaWindowSize), _sourceBlockSize(sourceBlockSize) { }
    xdelta3_builder(size_t bufferSize, const byte* payload) : filter_builder(bufferSize), _source(nullptr)
    {
      payload += sizeof(box::Payload);
      _sourceDigest = *(const box::DigestInfo*)payload;
      _xdeltaWindowSize = *(const box::length_t*)(payload + sizeof(box::DigestInfo));
      _sourceBlockSize = *(const box::length_t*)(payload + sizeof(box::DigestInfo) + sizeof(box::length_t));
    }
    
    void setup(const archive_environment& env) override;
    void unsetup(const archive_environment& env) override;
    
    box::payload_uid identifier() const override { return identifier::XDELTA3_FILTER; }
    std::string mnemonic() const override { return fmt::sprintf("xdelta3:source_size=%lu,source_crc32=%08X", _sourceDigest.size, _sourceDigest.crc32); }
    
    size_t payloadLength() const override { return sizeof(box::DigestInfo) + sizeof(box::length_t)*2; }
    memory_buffer payload() const override
    {
      memory_buffer buffer(sizeof(box::DigestInfo));
      buffer.write(_sourceDigest);
      buffer.write((box::length_t)_xdeltaWindowSize);
      buffer.write((box::length_t)_sourceBlockSize);
      return buffer;
    }
    
    data_source* apply(data_source* source) const override;
    data_source* unapply(data_source* source) const override;
  };
}
