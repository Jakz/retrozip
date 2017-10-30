#pragma once

#include "hash/hash.h"

namespace filters
{
  using data_filter = data_source;
  
  template<typename D>
  class digest_filter : public data_filter
  {
  private:
    const data_source* _source;
    std::function<void(const typename D::computed_type&)> _callback;
    mutable D _digester;
    
  public:
    digest_filter(const data_source* source) : _source(source), _callback([](const typename D::computed_type&){}), _digester() { }
    
    size_t read(void* dest, size_t size, size_t count) const override
    {
      size_t read = _source->read(dest, size, count);
      _digester.update((byte*)dest, read);
      return read;
    }
    
    bool eos() const override { return _source->eos(); }
    
    typename D::computed_type get() const { return _digester.get(); }
  };
  
  using crc32_filter = digest_filter<hash::crc32_digester>;
  using md5_filter = digest_filter<hash::md5_digester>;
  using sha1_filter = digest_filter<hash::sha1_digester>;
  
  class multiple_digest_filter : public data_filter
  {
  private:
    const data_source* _source;
    mutable hash::crc32_digester _crc32;
    mutable hash::md5_digester _md5;
    mutable hash::sha1_digester _sha1;
    
    bool _crc32enabled;
    bool _md5enabled;
    bool _sha1enabled;
    
  public:
    multiple_digest_filter(const data_source* source, bool crc32 = true, bool md5 = true, bool sha1 = true) :
    _source(source),_crc32(), _md5(), _sha1(),
    _crc32enabled(crc32), _md5enabled(md5), _sha1enabled(sha1)
    { }

    size_t read(void* dest, size_t size, size_t count) const override
    {
      size_t read = _source->read(dest, size, count);
      
      if (_crc32enabled) _crc32.update((byte*)dest, read);
      if (_md5enabled) _md5.update((byte*)dest, read);
      if (_sha1enabled) _sha1.update((byte*)dest, read);
      
      return read;
    }
    
    hash::crc32_t crc32() { assert(_crc32enabled); return _crc32.get(); }
    hash::md5_t md5() { assert(_md5enabled); return _md5.get(); }
    hash::sha1_t sha1() { assert(_sha1enabled); return _sha1.get(); }

    /*rzip::DigestInfo digest() const
    {
      rzip::DigestInfo digest;
      
      if (_crc32enabled) digest.crc32 = _crc32.get();
      if (_md5enabled) digest.md5 = _md5.get();
      if (_sha1enabled) digest.sha1 = _sha1.get();
      
      return digest;
    }*/
    
    bool eos() const override { return _source->eos(); }
  };
  
  class data_read_counter : public data_filter
  {
  private:
    const data_source* _source;
    mutable size_t _count;
    
  public:
    data_read_counter(const data_source* source) : _source(source), _count(0) { }
    
    void reset() { _count = 0; }
    size_t count() const { return _count; }
    
    bool eos() const override { return _source->eos(); }
    
    size_t read(void* dest, size_t size, size_t count) const override
    {
      size_t read = _source->read(dest, size, count);
      _count += read;
      return read;
    }
    
  };
  
  class data_write_counter : public data_sink
  {
  private:
    data_sink* _sink;
    mutable size_t _count;
    
  public:
    data_write_counter(data_sink* sink) : _sink(sink), _count(0) { }
    
    void reset() { _count = 0; }
    size_t count() const { return _count; }
    
    size_t write(const void* src, size_t size, size_t count) override
    {
      size_t written = _sink->write(src, size, count);
      _count += written;
      return written;
    }

  };
};
