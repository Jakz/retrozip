#include "base/common.h"
#include "base/exceptions.h"

#include "hash.h"

#include <cstring>
#include <iostream>

namespace hash
{
  namespace hidden
  {
    void SHA1::init()
    {
      /* SHA1 initialization constants */
      digest[0] = 0x67452301;
      digest[1] = 0xefcdab89;
      digest[2] = 0x98badcfe;
      digest[3] = 0x10325476;
      digest[4] = 0xc3d2e1f0;
      
      /* Reset counters */
      bufferSize = 0;
      transforms = 0;
    }
    
    void SHA1::transform(uint32_t* digest, uint32_t block[BLOCK_INTS])
    {
      u32 a = digest[0], b = digest[1], c = digest[2], d = digest[3], e = digest[4];
      
      /* 4 rounds of 20 operations each. Loop unrolled. */
      r0(block, a, b, c, d, e,  0);
      r0(block, e, a, b, c, d,  1);
      r0(block, d, e, a, b, c,  2);
      r0(block, c, d, e, a, b,  3);
      r0(block, b, c, d, e, a,  4);
      r0(block, a, b, c, d, e,  5);
      r0(block, e, a, b, c, d,  6);
      r0(block, d, e, a, b, c,  7);
      r0(block, c, d, e, a, b,  8);
      r0(block, b, c, d, e, a,  9);
      r0(block, a, b, c, d, e, 10);
      r0(block, e, a, b, c, d, 11);
      r0(block, d, e, a, b, c, 12);
      r0(block, c, d, e, a, b, 13);
      r0(block, b, c, d, e, a, 14);
      r0(block, a, b, c, d, e, 15);
      r1(block, e, a, b, c, d,  0);
      r1(block, d, e, a, b, c,  1);
      r1(block, c, d, e, a, b,  2);
      r1(block, b, c, d, e, a,  3);
      r2(block, a, b, c, d, e,  4);
      r2(block, e, a, b, c, d,  5);
      r2(block, d, e, a, b, c,  6);
      r2(block, c, d, e, a, b,  7);
      r2(block, b, c, d, e, a,  8);
      r2(block, a, b, c, d, e,  9);
      r2(block, e, a, b, c, d, 10);
      r2(block, d, e, a, b, c, 11);
      r2(block, c, d, e, a, b, 12);
      r2(block, b, c, d, e, a, 13);
      r2(block, a, b, c, d, e, 14);
      r2(block, e, a, b, c, d, 15);
      r2(block, d, e, a, b, c,  0);
      r2(block, c, d, e, a, b,  1);
      r2(block, b, c, d, e, a,  2);
      r2(block, a, b, c, d, e,  3);
      r2(block, e, a, b, c, d,  4);
      r2(block, d, e, a, b, c,  5);
      r2(block, c, d, e, a, b,  6);
      r2(block, b, c, d, e, a,  7);
      r3(block, a, b, c, d, e,  8);
      r3(block, e, a, b, c, d,  9);
      r3(block, d, e, a, b, c, 10);
      r3(block, c, d, e, a, b, 11);
      r3(block, b, c, d, e, a, 12);
      r3(block, a, b, c, d, e, 13);
      r3(block, e, a, b, c, d, 14);
      r3(block, d, e, a, b, c, 15);
      r3(block, c, d, e, a, b,  0);
      r3(block, b, c, d, e, a,  1);
      r3(block, a, b, c, d, e,  2);
      r3(block, e, a, b, c, d,  3);
      r3(block, d, e, a, b, c,  4);
      r3(block, c, d, e, a, b,  5);
      r3(block, b, c, d, e, a,  6);
      r3(block, a, b, c, d, e,  7);
      r3(block, e, a, b, c, d,  8);
      r3(block, d, e, a, b, c,  9);
      r3(block, c, d, e, a, b, 10);
      r3(block, b, c, d, e, a, 11);
      r4(block, a, b, c, d, e, 12);
      r4(block, e, a, b, c, d, 13);
      r4(block, d, e, a, b, c, 14);
      r4(block, c, d, e, a, b, 15);
      r4(block, b, c, d, e, a,  0);
      r4(block, a, b, c, d, e,  1);
      r4(block, e, a, b, c, d,  2);
      r4(block, d, e, a, b, c,  3);
      r4(block, c, d, e, a, b,  4);
      r4(block, b, c, d, e, a,  5);
      r4(block, a, b, c, d, e,  6);
      r4(block, e, a, b, c, d,  7);
      r4(block, d, e, a, b, c,  8);
      r4(block, c, d, e, a, b,  9);
      r4(block, b, c, d, e, a, 10);
      r4(block, a, b, c, d, e, 11);
      r4(block, e, a, b, c, d, 12);
      r4(block, d, e, a, b, c, 13);
      r4(block, c, d, e, a, b, 14);
      r4(block, b, c, d, e, a, 15);
      
      digest[0] += a;
      digest[1] += b;
      digest[2] += c;
      digest[3] += d;
      digest[4] += e;
      
      transforms++;
    }
    
    void SHA1::buffer_to_block(const u8* buffer, u32 block[BLOCK_INTS])
    {
      /* Convert the std::string (byte buffer) to a uint32_t array (MSB) */
      for (size_t i = 0; i < BLOCK_INTS; i++)
      {
        block[i] = (buffer[4*i+3] & 0xff)
        | (buffer[4*i+2] & 0xff)<<8
        | (buffer[4*i+1] & 0xff)<<16
        | (buffer[4*i+0] & 0xff)<<24;
      }
    }
    
    void SHA1::update(const void* data, size_t length)
    {
      const byte* bdata = reinterpret_cast<const byte*>(data);
      u32 block[BLOCK_INTS];
      
      size_t toFillBuffer = BLOCK_BYTES - bufferSize;
           
      while (length >= toFillBuffer)
      {
        if (toFillBuffer == BLOCK_BYTES)
          buffer_to_block(bdata, block);
        else
        {
          memcpy(&buffer[bufferSize], bdata, toFillBuffer);
          buffer_to_block(buffer, block);
          bufferSize = 0;
        }
        
        transform(digest, block);
        
        length -= toFillBuffer;
        bdata += toFillBuffer;
        toFillBuffer = BLOCK_BYTES;
      }
      
      assert(length + bufferSize < BLOCK_BYTES);
      if (length > 0)
      {
        memcpy(buffer + bufferSize, bdata, length);
        bufferSize += length;
      }
    }
    
    sha1_t SHA1::finalize()
    {
      u64 length = (transforms*BLOCK_BYTES + bufferSize) * 8;
      u32 block[BLOCK_INTS];

      /* append 1 bit then all 0s until last 64 that will be used for length in bits */
      buffer[bufferSize++] = 0x80;
      
      /* there is not enough space left in this block, hash it and pad with new one */
      if (bufferSize > BLOCK_BYTES - sizeof(u64))
      {
        memset(buffer+bufferSize, 0, BLOCK_BYTES - bufferSize);
        buffer_to_block(buffer, block);
        transform(digest, block);
        bufferSize = 0;
      }
      
      memset(buffer+bufferSize, 0, BLOCK_BYTES - bufferSize - sizeof(u64));

      buffer_to_block(buffer, block);
      block[BLOCK_INTS - 1] = length & 0xFFFFFFFF;
      block[BLOCK_INTS - 2] = (length >> 32) & 0xFFFFFFFF;
      transform(digest, block);
      
      sha1_t result;
      for (size_t i = 0; i < 5; ++i)
      {
        result.inner()[i*4] = (digest[i] >> 24) & 0xFF;
        result.inner()[i*4 + 1] = (digest[i] >> 16) & 0xFF;
        result.inner()[i*4 + 2] = (digest[i] >> 8) & 0xFF;
        result.inner()[i*4 + 3] = (digest[i] >> 0) & 0xFF;
      }
      
      return result;
    }
  }
  
}
