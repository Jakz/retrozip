#pragma once

#include <cstdint>

#ifdef BOXLIB_EXPORTS
#define BOXLIB_API __declspec(dllexport)
#else
#define BOXLIB_API __declspec(dllimport)
#endif

using archive_handle = void*;
using byte = uint8_t;
using crc32_t = uint64_t;

enum class BoxResult
{
  Success = 0,
  
  EntryIsOutsideRange = -1
};

struct ArchiveInfo
{
  size_t entryCount;
  size_t streamCount;
};

struct EntryInfo
{
  const char* name;
  
  /* size */
  size_t size;
  size_t filteredSize;

  /* digest info */
  crc32_t crc32;
  byte* md5;
  byte* sha1;
};

extern "C"
{
  BOXLIB_API archive_handle* boxOpenArchive(const char* path);
  BOXLIB_API void boxCloseArchive(archive_handle* handle);
  
  BOXLIB_API void boxFillArchiveInfo(archive_handle* handle, ArchiveInfo* dest);
  BOXLIB_API ArchiveInfo boxGetArchiveInfo(archive_handle* handle);

  BOXLIB_API void boxFillEntryInfo(archive_handle* handle, size_t entryIndex, EntryInfo* info);

  BOXLIB_API BoxResult boxExtractEntryFromArchive(archive_handle* handle, size_t entryIndex, const char* destination);

}