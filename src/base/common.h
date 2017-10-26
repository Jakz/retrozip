#pragma once

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
  struct u16
  {
    u16(::u16 v) : data(v) { }
    operator ::u16() const { return data; }
  private:
    ::u16 data;
  };
  struct u16re
  {
    u16re(::u16 v) : data(v) { }
    operator ::u16() const
    {
      ::u16 v = ((data & 0xFF) << 8) | ((data & 0xFF00) >> 8);
      return v;
    }
  private:
    ::u16 data;
  };
}

using u16le = std::conditional<IS_LITTLE_ENDIAN_, hidden::u16, hidden::u16re>::type;
using u16be = std::conditional<IS_LITTLE_ENDIAN_, hidden::u16re, hidden::u16>::type;

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


constexpr size_t KB16 = 16384;

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
