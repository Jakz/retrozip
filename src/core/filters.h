#pragma once

#include "header.h"

namespace filters
{
  template<typename D>
  class digest_filter : public data_source
  {
  private:
    const data_source* _source;
    mutable D _digester;
    
  public:
    digest_filter(const data_source* source) : _source(source), _digester() { }
    
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
  
  
};
