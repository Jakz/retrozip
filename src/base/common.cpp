#include "common.h"

#include <zlib.h>

std::string strings::humanReadableSize(size_t bytes, bool si) {
  static constexpr char pre[][7] = { "kMGTPE", "KMGTPE"};
  
  
  int unit = si ? 1000 : 1024;
  if (bytes < unit) return std::to_string(bytes) + " B";
  int exp = std::log(bytes) / std::log(unit);
  
  return fmt::sprintf("%.1f %c%sB", bytes / std::pow(unit, exp), pre[si ? 1 : 0][exp-1], si ? "" : "i");
}

bool strings::isPrefixOf(const std::string& string, const std::string& prefix)
{
  return std::mismatch(prefix.begin(), prefix.end(), string.begin()).first == prefix.end();
}



enum class ZlibResult : int
{
  OK = Z_OK,
  DATA_ERROR = Z_DATA_ERROR,
  MEM_ERROR = Z_MEM_ERROR,
  NEED_DICT = Z_NEED_DICT,
  STREAM_END = Z_STREAM_END
};

int inflate(byte* src, size_t length, byte* dest, size_t destLength)
{
  z_stream strm;
  strm.zalloc = Z_NULL;
  strm.zfree = Z_NULL;
  strm.opaque = Z_NULL;
  strm.total_out = 0;
  strm.avail_in = static_cast<unsigned>(length);
  strm.total_in = strm.avail_in;
  strm.next_in = src;
  
  int r = inflateInit2(&strm, -15);
  
  if (r != Z_OK)
    return r;
  
  strm.next_out = dest;
  strm.avail_out = static_cast<uint32_t>(destLength);
  
  do
  {
    r = inflate(&strm, Z_FINISH);
    
    switch (r)
    {
      case Z_NEED_DICT: r = Z_DATA_ERROR;
      case Z_DATA_ERROR:
      case Z_MEM_ERROR:
        inflateEnd(&strm);
        return r;
    }
  } while (r != Z_STREAM_END);
  
  inflateEnd(&strm);
  return r == Z_STREAM_END ? Z_OK : Z_DATA_ERROR;
}
