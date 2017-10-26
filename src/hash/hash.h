#pragma once

#include "base/common.h"
#include <array>

class path;

namespace hash
{
  using crc32_t = u32;
  
  struct crc32_digester
  {
  private:
    static void precomputeLUT();
    
    crc32_t value;
    
    crc32_t update(const void* data, size_t length, crc32_t previous);
    
  public:
    crc32_digester() : value(0) { precomputeLUT(); }
    void update(const void* data, size_t length);
    crc32_t get() const { return value; }
    
    static crc32_t compute(const void* data, size_t length);
    static crc32_t compute(const class path& path);
  };

  /* forward declaration for friend directive */
  namespace hidden { class MD5; }
  
  struct md5_t
  {
  public:
    constexpr static size_t length = 16;

  private:
    std::array<byte, length> data;
    
  public:
    md5_t() : data({{0}}) { }
    md5_t(const std::array<byte, length>& data) : data(data) { }
    
    const byte& operator[](size_t index) const { return data[index]; }
    byte& operator[](size_t index) { return data[index]; }
    
    operator std::string() const;
    
    std::ostream& operator<<(std::ostream& o) const { o << operator std::string(); return o; }
    bool operator==(const std::string& string) const { return operator std::string() == string; }
    
    friend class hidden::MD5;
  };
  
  namespace hidden
  {
    class MD5
    {
    private:
      union aliased_uint32
      {
        u32 value;
        u8 bytes[4];
      };
      
      struct aliased_block
      {
        aliased_uint32 values[16];
        u32 operator[](size_t index) const { return values[index].value; }
      };
      
      using block_t = std::conditional<IS_LITTLE_ENDIAN_, aliased_block, u8[64]>::type;
      
    public:
      using size_type = uint32_t;
      
      MD5() { init(); }
      void update(const void *buf, size_t length);
      md5_t finalize();
      
    private:
      static constexpr u32 blocksize = 64;
      
      bool finalized;
      u8 buffer[blocksize]; // bytes that didn't fit in last 64 byte chunk
      u32 count[2];   // 64bit counter for number of bits (lo, hi)
      u32 state[4];   // digest so far
      md5_t digest; // the result
      
    private:
      
      void init();
      
      void transform(const u8 block[blocksize]);
      static void decode(u32* output, const u8* input, size_t len);
      static void encode(u8* output, const u32* input, size_t len);
      
      static inline u32 F(u32 x, u32 y, u32 z) { return (x&y) | (~x&z); }
      static inline u32 G(u32 x, u32 y, u32 z) { return (x&z) | (y&~z); }
      static inline u32 H(u32 x, u32 y, u32 z) { return x^y^z; }
      static inline u32 I(u32 x, u32 y, u32 z) { return y ^ (x | ~z); }
      static inline u32 rotate_left(u32 x, int n) { return (x << n) | (x >> (32-n)); }
      
      using md5_functor = u32(*)(u32, u32, u32);
      template<md5_functor F>
      static inline void rf(u32& a, u32 b, u32 c, u32 d, u32 x, u32 s, u32 ac)
      {
        a = rotate_left(a + F(b, c, d) + x + ac, s) + b;
      }
    };
  }
  
  struct md5_digester
  {
  private:
    hidden::MD5 impl;
    
  public:
    md5_digester() { }
    void update(const void* data, size_t length);
    md5_t get() { return impl.finalize(); }
    
    static md5_t compute(const void* data, size_t length);
    static md5_t compute(const class path& path);
  };
}
