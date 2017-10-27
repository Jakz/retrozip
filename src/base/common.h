#pragma once

#define _FILE_OFFSET_BITS 64
#include <cstdint>
#include <cmath>

#include <string>

#include "libs/fmt/printf.h"
#include "exceptions.h"
#include "path.h"



using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using byte = u8;
using offset_t = u32;

using s64 = int64_t;

#if defined(__BYTE_ORDER) && __BYTE_ORDER == __BIG_ENDIAN || \
\
defined(__BIG_ENDIAN__) || \
defined(__ARMEB__) || \
defined(__THUMBEB__) || \
defined(__AARCH64EB__) || \
defined(_MIBSEB) || defined(__MIBSEB) || defined(__MIBSEB__)

#define IS_BIG_ENDIAN
constexpr bool IS_LITTLE_ENDIAN_ = false;

#elif defined(__BYTE_ORDER) && __BYTE_ORDER == __LITTLE_ENDIAN || \
\
defined(__LITTLE_ENDIAN__) || \
defined(__ARMEL__) || \
defined(__THUMBEL__) || \
defined(__AARCH64EL__) || \
defined(_MIPSEL) || defined(__MIPSEL) || defined(__MIPSEL__)

#define IS_LITTLE_ENDIAN
constexpr bool IS_LITTLE_ENDIAN_ = true;

#else
#error "Unknown endianness!"
#endif

namespace hidden
{
  struct u16se
  {
    u16se(u16 v) : data(v) { }
    operator u16() const { return data; }
    u16se& operator=(u16 v) { this->data = v; return *this; }
  private:
    u16 data;
  };
  
  struct u16re
  {
    u16re(u16 v) { operator=(v); }
    
    operator u16() const
    {
      u16 v = ((data & 0xFF) << 8) | ((data & 0xFF00) >> 8);
      return v;
    }
    
    u16re& operator=(u16 v)
    {
      this->data = ((v & 0xFF) << 8) | ((v >> 8) & 0xFF);
      return *this;
    }
    
  private:
    u16 data;
  };
  
  struct u32se
  {
    u32se(u32 v) : data(v) { }
    operator u32() const { return data; }
    u32se& operator=(u32 v) { this->data = v; return *this; }
  private:
    u32 data;
  };
  
  struct u32re
  {
    u32re(u32 v) { operator=(v); }
    
    operator u32() const
    {
      u32 vv =
        ((data & 0xFF) << 24) |
        ((data & 0xFF00) << 8) |
        ((data & 0xFF0000) >> 8) |
        ((data & 0xFF000000) >> 24);
      return vv;
    }
    
    u32re& operator=(u32 v)
    {
      this->data =
      ((v & 0xFF) << 24) |
      ((v & 0xFF00) << 8) |
      ((v & 0xFF0000) >> 8) |
      ((v & 0xFF000000) >> 24);
      
      return *this;
    }
    
  private:
    ::u32 data;
  };
}

using u16le = std::conditional<IS_LITTLE_ENDIAN_, hidden::u16se, hidden::u16re>::type;
using u16be = std::conditional<IS_LITTLE_ENDIAN_, hidden::u16re, hidden::u16se>::type;
using u32le = std::conditional<IS_LITTLE_ENDIAN_, hidden::u32se, hidden::u32re>::type;
using u32be = std::conditional<IS_LITTLE_ENDIAN_, hidden::u32re, hidden::u32se>::type;

using u32se = std::conditional<IS_LITTLE_ENDIAN_, u32le, u32be>::type;
using u32de = std::conditional<IS_LITTLE_ENDIAN_, u32be, u32le>::type;


struct u32_optional
{
private:
  u64 data;
  
public:
  u32_optional() : data(0xFFFFFFFFFFFFFFFFLL) { }
  u32_optional(u32 data) : data(data) { }
  
  bool isPresent() const { return (data & 0xFFFFFFFF) != 0; }
  void set(u32 data) { this->data = data; }
};

template<size_t LENGTH>
struct wrapped_array
{
private:
  std::array<byte, LENGTH> data;
  
public:
  wrapped_array() : data({{0}}) { }
  wrapped_array(const std::array<byte, LENGTH>& data) : data(data) { }
  
  const byte* inner() const { return data.data(); }
  byte* inner() { return data.data(); }
  
  const byte& operator[](size_t index) const { return data[index]; }
  byte& operator[](size_t index) { return data[index]; }
  
  operator std::string() const
  {
    constexpr bool uppercase = false;
    
    char buf[LENGTH*2+1];
    for (size_t i = 0; i < LENGTH; i++)
      sprintf(buf+i*2, uppercase ? "%02X" : "%02x", data[i]);
    
    buf[LENGTH*2] = '\0';
    
    return std::string(buf);
  }
  
  std::ostream& operator<<(std::ostream& o) const { o << operator std::string(); return o; }
  bool operator==(const std::string& string) const { return operator std::string() == string; }
};

template<typename T>
struct bit_mask
{
  using utype = typename std::underlying_type<T>::type;
  utype value;
  
  bool isSet(T flag) const { return value & static_cast<utype>(flag); }
  void set(T flag) { value |= static_cast<utype>(flag); }
  void reset(T flag) { value &= ~static_cast<utype>(flag); }
  
  bit_mask<T>& operator&(T flag)
  {
    bit_mask<T> mask;
    mask.value = this->value & static_cast<utype>(flag);
    return mask;
  }
  
  bit_mask<T>& operator|(T flag)
  {
    bit_mask<T> mask;
    mask.value = this->value | static_cast<utype>(flag);
    return mask;
  }
};

constexpr size_t KB16 = 16384;
constexpr size_t KB64 = 16384 << 2;

enum class ZlibResult : int;

namespace utils
{
  std::string humanReadableSize(size_t bytes, bool si);
  
  int inflate(byte* src, size_t length, byte* dest, size_t destLength);
}

namespace strings
{
  std::string humanReadableSize(size_t bytes, bool si);
  bool isPrefixOf(const std::string& string, const std::string& prefix);
}
