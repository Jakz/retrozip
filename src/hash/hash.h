#pragma once

#include "base/common.h"
#include <array>

class path;

namespace hash
{
  /* CRC32 */
  using crc32_t = u32;
  
  struct crc32_digester
  {
  private:
    static void precomputeLUT();
    
    crc32_t value;

    crc32_t update(const void* data, size_t length, crc32_t previous);
    
  public:
    using computed_type = crc32_t;
    
    crc32_digester() : value(0) { precomputeLUT(); }
    void update(const void* data, size_t length);
    crc32_t get() const { return value; }
    
    static crc32_t compute(const void* data, size_t length);
    static crc32_t compute(const class path& path);
  };

  /* MD5 */
  using md5_t = wrapped_array<16>;
  
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
    using computed_type = md5_t;
    
    md5_digester() { }
    void update(const void* data, size_t length);
    md5_t get() { return impl.finalize(); }
    
    static md5_t compute(const void* data, size_t length);
  };
  
  /* SHA-1 */
  using sha1_t = wrapped_array<20>;
  
  namespace hidden
  {
    class SHA1
    {
    private:
      static constexpr size_t BLOCK_INTS = 16;  /* number of 32bit integers per SHA1 block */
      static constexpr size_t BLOCK_BYTES = BLOCK_INTS * 4;
      
      uint32_t digest[5];
      size_t bufferSize;
      byte buffer[BLOCK_BYTES];
      uint64_t transforms;
      
    private:      
      static inline u32 rol(const u32 value, const size_t bits) { return (value << bits) | (value >> (32 - bits)); }
      static inline u32 blk(const u32 block[BLOCK_INTS], const size_t i) { return rol(block[(i+13)&15] ^ block[(i+8)&15] ^ block[(i+2)&15] ^ block[i], 1); }
      
      static inline u32 F(u32 w, u32 x, u32 y) { return (w&(x^y))^y; }
      static inline u32 G(u32 w, u32 x, u32 y) { return (w^x^y); }
      static inline u32 H(u32 w, u32 x, u32 y) { return ((w|x)&y)|(w&x); }
      using sha1_functor = u32(*)(u32,u32,u32);
      
      template<sha1_functor F, u32 MAGIC>
      static inline void rf(u32 block[BLOCK_INTS], const u32 v, u32& w, u32 x, u32 y, u32& z, size_t i)
      {
        z += F(w,x,y) + block[i] + MAGIC + rol(v, 5);
        w = rol(w, 30);
      }
      
      template<sha1_functor F, u32 MAGIC>
      static inline void rn(u32 block[BLOCK_INTS], const u32 v, u32& w, u32 x, u32 y, u32& z, size_t i)
      {
        block[i] = blk(block, i);
        rf<F, MAGIC>(block, v, w, x, y, z, i);
      }
      
      static inline void r0(u32 block[BLOCK_INTS], const u32 v, u32& w, u32 x, u32 y, u32& z, size_t i) { rf<F, 0x5a827999>(block, v, w, x, y, z, i); }
      static inline void r1(u32 block[BLOCK_INTS], const u32 v, u32& w, u32 x, u32 y, u32& z, size_t i) { rn<F, 0x5a827999>(block, v, w, x, y, z, i); }
      static inline void r2(u32 block[BLOCK_INTS], const u32 v, u32& w, u32 x, u32 y, u32& z, size_t i) { rn<G, 0x6ed9eba1>(block, v, w, x, y, z, i); }
      static inline void r3(u32 block[BLOCK_INTS], const u32 v, u32& w, u32 x, u32 y, u32& z, size_t i) { rn<H, 0x8f1bbcdc>(block, v, w, x, y, z, i); }
      static inline void r4(u32 block[BLOCK_INTS], const u32 v, u32& w, u32 x, u32 y, u32& z, size_t i) { rn<G, 0xca62c1d6>(block, v, w, x, y, z, i); }
      
      void buffer_to_block(const u8* buffer, u32 block[BLOCK_INTS]);
      void transform(u32* digest, u32 block[BLOCK_INTS]);
      void init();

    public:
      SHA1() { init(); }
      void update(const void* data, size_t length);
      sha1_t finalize();
    };
  }
  
  struct sha1_digester
  {
  private:
    hidden::SHA1 impl;
    
  public:
    using computed_type = sha1_t;

    
    sha1_digester() { }
    void update(const void* data, size_t length) { impl.update(data, length); }
    sha1_t get() { return impl.finalize(); }
    
    static sha1_t compute(const void* data, size_t length)
    {
      hidden::SHA1 sha1;
      sha1.update(data, length);
      return sha1.finalize();
    }
  };
  
}
