#pragma once

#include "hash/hash.h"
#include "core/data_filter.h"

namespace filters
{
  template<typename D>
  class digest_filter : public unbuffered_data_filter
  {
  private:
    std::function<void(const typename D::computed_type&)> _callback;
    mutable D _digester;
    
  public:
    digest_filter() : _callback([](const typename D::computed_type&){}), _digester() { }
    
    void process(const byte* data, size_t amount, size_t effective) override
    {
      if (effective != END_OF_STREAM)
        _digester.update(data, effective);
    }

    typename D::computed_type get() const { return _digester.get(); }
  };

  
  using crc32_filter = digest_filter<hash::crc32_digester>;
  using md5_filter = digest_filter<hash::md5_digester>;
  using sha1_filter = digest_filter<hash::sha1_digester>;
  
  class multiple_digest_filter : public unbuffered_data_filter
  {
  private:
    mutable hash::crc32_digester _crc32;
    mutable hash::md5_digester _md5;
    mutable hash::sha1_digester _sha1;
    
    bool _crc32enabled;
    bool _md5enabled;
    bool _sha1enabled;
    
  public:
    multiple_digest_filter(bool crc32 = true, bool md5 = true, bool sha1 = true) :
    _crc32(), _md5(), _sha1(),
    _crc32enabled(crc32), _md5enabled(md5), _sha1enabled(sha1)
    { }

    void process(const byte* data, size_t amount, size_t effective) override
    {
      if (_crc32enabled) _crc32.update(data, effective);
      if (_md5enabled) _md5.update(data, effective);
      if (_sha1enabled) _sha1.update(data, effective);
    }
    
    hash::crc32_t crc32() { assert(_crc32enabled); return _crc32.get(); }
    hash::md5_t md5() { assert(_md5enabled); return _md5.get(); }
    hash::sha1_t sha1() { assert(_sha1enabled); return _sha1.get(); }
  };
  
  class data_counter : public unbuffered_data_filter
  {
  private:
    mutable size_t _count;
    
  public:
    data_counter() : _count(0) { }
    
    void reset() { _count = 0; }
    size_t count() const { return _count; }
        
    void process(const byte* data, size_t amount, size_t effective) override
    {
      if (effective != END_OF_STREAM)
        _count += effective;
    }
    
  };
};
