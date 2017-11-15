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
      if (effective != END_OF_STREAM)
      {
        if (_crc32enabled) _crc32.update(data, effective);
        if (_md5enabled) _md5.update(data, effective);
        if (_sha1enabled) _sha1.update(data, effective);
      }
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
  
  
  /* simple xor encryption */
  class xor_filter : public data_filter
  {
  private:
    std::vector<byte> _key;
    size_t _counter;
  public:
    xor_filter(size_t bufferSize, const std::vector<byte>& key) : data_filter(bufferSize, bufferSize), _key(key), _counter(0) { }
    
    xor_filter(size_t bufferSize, const byte* key, size_t length) : data_filter(bufferSize, bufferSize), _counter(0)
    {
      _key.resize(length);
      std::copy(key, key + length, _key.begin());
    }
    
    void init() override { }
    void finalize() override { }
    
    void process() override
    {
      size_t effective = std::min(_in.used(), _out.available());
      
      std::copy(_in.head(), _in.head() + effective, _out.tail());
      
      byte* ptr = _out.tail();
      for (size_t i = 0; i < effective; ++i)
      {
        *ptr ^= _key[_counter++];
        ++ptr;
        _counter %= _key.size();
      }
      
      _in.consume(effective);
      _out.advance(effective);
      
      if (ended() && _in.empty() && _out.empty())
        _finished = true;
    }
    
    std::string name() override { return "xor"; }
  };
  
  /* skip filter which skips specific amount of bytes before reading/writing */
  /* TODO: this uses buffers but it's not necessary, but there is no data_filter interface without
     buffers and unbuffered_data_filter interface doesn't allow doing this */
  class skip_filter : public data_filter
  {
  private:
    size_t _amountToSkip;
    size_t _amountToPassThrough;
    size_t _offset;
    
    size_t _position;
    size_t _skipped;
    size_t _passed;
    
    size_t passthrough(size_t amount)
    {
      size_t effective = std::min(std::min(_in.used(), _out.available()), amount);
      if (effective > 0)
      {
        std::copy(_in.head(), _in.head() + effective, _out.tail());
        _in.consume(effective);
        _out.advance(effective);
      }
      return effective;
    }
    
  public:
    skip_filter(size_t bufferSize, size_t amountToSkip, size_t amountToPassThrough = 0, size_t offset = 0) : data_filter(bufferSize),
      _amountToSkip(amountToSkip), _amountToPassThrough(amountToPassThrough), _offset(offset), _position(0), _skipped(0), _passed(0) { }
    
    void init() override { _skipped = 0; }
    void finalize() override { }
    void process() override
    {
      /* we still have to reach the position in which start skipping */
      if (_position < _offset)
      {
        size_t amount = std::min(_in.used(), _offset - _position);
        if (amount > 0)
          _position += passthrough(amount);
          
        TRACE_IF(amount > 0, "%p: skip_filter::process() passing %lu bytes through before skipping", this, amount);
        if (!_in.empty()) process();
      }
      /* we already skipped so we can passthrough */
      else if (_skipped == _amountToSkip)
      {
        size_t effective = 0;
        
        if (_passed < _amountToPassThrough)
        {
          size_t amount = _amountToPassThrough - _passed;
          effective = passthrough(amount);
          TRACE_IF(effective > 0, "%p: skip_filter::process() passing %lu limiting bytes through after skipping", this, effective);
          _passed += effective;
        }
        else if (_amountToPassThrough != 0)
        {
          effective = _in.used();
          _in.consume(_in.used());
          
          TRACE_IF(effective > 0, "%p: skip_filter::process() BLA BLA skipping %lu bytes", this, effective);
          this->_finished = _out.empty() && ended();
        }
        else
        {
          effective = passthrough(std::numeric_limits<size_t>::max());
          TRACE_IF(effective > 0, "%p: skip_filter::process() passing %lu bytes through after skipping", this, effective);
        }
        
        _position += effective;
        
        if (!_in.empty()) process();
      }
      /* otherwise we must skip */
      else
      {
        size_t stillToSkip = std::min(_amountToSkip - _skipped, _in.used());
        
        _skipped += stillToSkip;
        _in.consume(stillToSkip);
        
        TRACE_IF(stillToSkip > 0, "%p: skip_filter::process() skipping %lu bytes", this, stillToSkip);

        if (!_in.empty()) process();
      }

      this->_finished = _skipped == _amountToSkip && _passed >= _amountToPassThrough && _in.empty() && _out.empty() && ended();
    }
    
    std::string name() override { return "skip"; }
  };
  
  
};
