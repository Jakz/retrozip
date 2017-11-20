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

using s32 = int32_t;
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

#ifdef DEBUG
extern void debugprintf(const char* str, ...);
extern void debugnnprintf(const char* str, ...);
#define LOG(...) debugprintf(__VA_ARGS__)
#else
#define LOG(...) do { } while (false)
#endif

#define TRACE_MEMORY_BUFFERS 0
#define TRACE_PIPES 0
#define TRACE_ARCHIVE 0
#define TRACE_ENABLED 0

#define TRACE_FORCE_DISABLE 0


#if defined(TRACE_FORCE_DISABLE) && TRACE_FORCE_DISABLE == 1
#define TRACEL(...) do { } while (false)
#else
#define TRACEL LOG
#endif

#if defined(TRACE_MEMORY_BUFFERS) && TRACE_MEMORY_BUFFERS == 1
#define TRACE_MB TRACEL
#else
#define TRACE_MB(...) do { } while (false)
#endif

#if defined(TRACE_PIPES) && TRACE_PIPES == 1
#define TRACE_P TRACEL
#else
#define TRACE_P(...) do { } while (false)
#endif

#if defined(TRACE_ARCHIVE) && TRACE_ARCHIVE >= 1
#define TRACE_A TRACEL
#if TRACE_ARCHIVE >= 2
#define TRACE_A2 TRACEL
#endif
#else
#define TRACE_A(...) do { } while (false)
#endif

#if !defined(TRACE_A2)
#define TRACE_A2(...) do { } while (false)
#endif

#if defined(TRACE_ENABLED) && TRACE_ENABLED == 1
#define TRACE TRACEL
#else
#define TRACE(...) do { } while (false)
#endif

#define TRACE_IF(c, ...) if (c) { TRACE(__VA_ARGS__); }

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
    u32se() { }
    u32se(u32 v) : data(v) { }
    operator u32() const { return data; }
    u32se& operator=(u32 v) { this->data = v; return *this; }
  private:
    u32 data;
  };
  
  struct u32re
  {
    u32re() { }
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
    u32 data;
  };
}

using u16le = std::conditional<IS_LITTLE_ENDIAN_, hidden::u16se, hidden::u16re>::type;
using u16be = std::conditional<IS_LITTLE_ENDIAN_, hidden::u16re, hidden::u16se>::type;
using u32le = std::conditional<IS_LITTLE_ENDIAN_, hidden::u32se, hidden::u32re>::type;
using u32be = std::conditional<IS_LITTLE_ENDIAN_, hidden::u32re, hidden::u32se>::type;

using u16se = std::conditional<IS_LITTLE_ENDIAN_, u16le, u16be>::type;
using u16de = std::conditional<IS_LITTLE_ENDIAN_, u16be, u16le>::type;
using u32se = std::conditional<IS_LITTLE_ENDIAN_, u32le, u32be>::type;
using u32de = std::conditional<IS_LITTLE_ENDIAN_, u32be, u32le>::type;

namespace strings
{
  std::string humanReadableSize(size_t bytes, bool si);
  bool isPrefixOf(const std::string& string, const std::string& prefix);
  
  std::vector<byte> toByteArray(const std::string& string);
  std::string fromByteArray(const byte* data, size_t length);
  inline std::string fromByteArray(const std::vector<byte>& data) { return fromByteArray(data.data(), data.size()); }

}

template<typename T>
struct optional
{
private:
  T _data;
  bool _isPresent;
  
public:
  optional(T data) : _data(data), _isPresent(true) { }
  optional() : _data(), _isPresent(false) { }
  
  bool isPresent() const { return _isPresent; }
  void set(T data) { this->_data = data; _isPresent = true; }
  void clear() { _isPresent = false; }
  
  T get() const { assert(_isPresent); return _data; }
};


template<>
struct optional<u32>
{
private:
  static constexpr u64 EMPTY_VALUE = 0xFFFFFFFFFFFFFFFFLL;
  u64 _data;
  
public:
  optional<u32>() : _data(EMPTY_VALUE) { }
  optional<u32>(u32 data) : _data(data) { }
  
  bool isPresent() const { return ((_data >> 32) & 0xFFFFFFFF) == 0; }
  void set(u32 data) { this->_data = data; }
  void clear() { this->_data = EMPTY_VALUE; }
  
  u32 get() const { return _data & 0xFFFFFFFF; }
};

template <bool B, typename T, T trueval, T falseval>
struct conditional_value : std::conditional<B, std::integral_constant<T, trueval>, std::integral_constant<T, falseval>>::type { };

template<size_t LENGTH>
struct wrapped_array
{
private:
  std::array<byte, LENGTH> _data;
  
public:
  wrapped_array() : _data({{0}}) { }
  wrapped_array(const std::array<byte, LENGTH>& data) : _data(data) { }
  wrapped_array(const byte* data) {
    std::copy(data, data + LENGTH, _data.begin());
  }
  
  const byte* inner() const { return _data.data(); }
  byte* inner() { return _data.data(); }
  
  const byte& operator[](size_t index) const { return _data[index]; }
  byte& operator[](size_t index) { return _data[index]; }
  
  operator std::string() const
  {
    return strings::fromByteArray(_data.data(), LENGTH);
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
  
  bool operator&&(T flag) const { return (value & static_cast<utype>(flag)) != 0; }
};

constexpr size_t KB8 = 8192;
constexpr size_t KB16 = 16384;
constexpr size_t KB32 = 16384 << 1;
constexpr size_t KB64 = 16384 << 2;
constexpr size_t KB128 = KB64 << 1;
constexpr size_t KB256 = KB128 << 1;
constexpr size_t MB1 = 1 << 20;
constexpr size_t MB2 = MB1 << 1;
constexpr size_t MB4 = MB1 << 2;
constexpr size_t MB8 = MB1 << 3;
constexpr size_t MB16 = MB8 << 1;
constexpr size_t MB32 = MB8 << 2;
constexpr size_t MB64 = MB8 << 3;
constexpr size_t MB128 = MB8 << 4;
constexpr size_t MB256 = MB8 << 5;

enum class ZlibResult : int;

namespace utils
{
  int inflate(byte* src, size_t length, byte* dest, size_t destLength);
  
  u64 random64(u64 first, u64 last);
  
  inline static u64 nextPowerOfTwo(u64 v)
  {
    v--; v |= v >> 1; v |= v >> 2; v |= v >> 4;
    v |= v >> 8; v |= v >> 16; v |= v >> 32;
    v++;
    return v;
  }
}
