#pragma once

#include "base/common.h"

#include <array>

namespace crypto
{
  enum class AESKeyLength
  {
    _128 = 16,
    _192 = 24,
    _256 = 32
  };
  
  enum class AESMode
  {
    CBC,
    ECB
  };
    
  template<AESKeyLength TYPE, AESMode MODE>
  class AES
  {
  private:
    static constexpr size_t KEYLEN = static_cast<size_t>(TYPE);
    
    /* number of columns */
    static constexpr size_t Nb = 4;
    /* key length in words */
    static constexpr size_t Nk = KEYLEN / 4;
    /* round amount 10, 12 or 14 */
    static constexpr size_t Nr = conditional_value<TYPE == AESKeyLength::_128, size_t, 10, conditional_value<TYPE == AESKeyLength::_192, size_t, 12, 14>::value>::value;
    static constexpr size_t keyExpSize = conditional_value<TYPE == AESKeyLength::_128, size_t, 176, conditional_value<TYPE == AESKeyLength::_192, size_t, 208, 240>::value>::value;
    
    using iv_t = std::conditional<MODE == AESMode::CBC, wrapped_array<static_cast<size_t>(TYPE)>, void>;
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
    
  public:
    
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
  };
  
  using AES128 = AES<AESKeyLength::_128, AESMode::ECB>;
  
}
