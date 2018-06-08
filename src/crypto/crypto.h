#pragma once

#include "tbx/base/common.h"

#include <array>

namespace crypto
{
  enum class AESKeyLength
  {
    _128 = 16,
    _192 = 24,
    _256 = 32
  };
    
  template<AESKeyLength TYPE>
  class AES
  {
  public:
    static constexpr size_t BLOCKLEN = 16;
    static constexpr size_t KEYLEN = static_cast<size_t>(TYPE);
    
    /* number of columns */
    static constexpr size_t Nb = 4;
    /* key length in words */
    static constexpr size_t Nk = KEYLEN / 4;
    /* round amount 10, 12 or 14 */
    static constexpr size_t Nr = conditional_value<TYPE == AESKeyLength::_128, size_t, 10, conditional_value<TYPE == AESKeyLength::_192, size_t, 12, 14>::value>::value;
    static constexpr size_t keyExpSize = conditional_value<TYPE == AESKeyLength::_128, size_t, 176, conditional_value<TYPE == AESKeyLength::_192, size_t, 208, 240>::value>::value;
    
    using iv_t = wrapped_array<BLOCKLEN>;
    using key_t = wrapped_array<KEYLEN>;
    using state_t = std::array<std::array<u8, 4>, 4>;

  private:
    state_t* state;
    key_t key;
    iv_t iv;
    u8 RoundKey[keyExpSize];
    
    
    inline uint8_t xtime(uint8_t x) { return ((x<<1) ^ (((x>>7) & 1) * 0x1b)); }
    inline uint8_t Multiply(uint8_t x, uint8_t y)
    {
      return (((y & 1) * x) ^
              ((y>>1 & 1) * xtime(x)) ^
              ((y>>2 & 1) * xtime(xtime(x))) ^
              ((y>>3 & 1) * xtime(xtime(xtime(x)))) ^
              ((y>>4 & 1) * xtime(xtime(xtime(xtime(x))))));
    }
    
  private:
    void keyExpansion();
    void addRoundKey(u8 round);
    
    void subBytes();
    void invSubBytes();
    
    void shiftRows();
    void invShiftRows();
    
    void mixColumns();
    void invMixColumns();
    
    void cipher();
    void decipher();
    
    void prepare(const byte* input, byte* output, const byte* key, size_t length)
    {
      memcpy(output, input, length);
      
      this->state = reinterpret_cast<state_t*>(output);
      this->key = key;
      
      keyExpansion();
    }
    
    template<bool ENCRYPT> void cbc(const byte* input, byte* output, const byte* key, size_t length, const byte* iv);

    
  public:
    
    size_t key_length() const { return KEYLEN; }
    
    void crypt(const byte* input, byte* output, const byte* key, size_t length)
    {
      prepare(input, output, key, length);
      cipher();
    }
    
    void decrypt(const byte* input, byte* output, const byte* key, size_t length)
    {
      prepare(input, output, key, length);
      decipher();
    }
    
    void cryptBuffer(const byte* input, byte* output, const byte* key, size_t length, const byte* iv)
    {
      cbc<true>(input, output, key, length, iv);
    }
    
    void decryptBuffer(const byte* input, byte* output, const byte* key, size_t length, const byte* iv)
    {
      cbc<false>(input, output, key, length, iv);
    }
  };
  
  using AES128 = AES<AESKeyLength::_128>;
  using AES192 = AES<AESKeyLength::_192>;
  using AES256 = AES<AESKeyLength::_256>;
}
