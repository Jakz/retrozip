#pragma once

#ifdef BOXLIB_EXPORTS
#define BOXLIB_API __declspec(dllexport)
#else
#define BOXLIB_API __declspec(dllimport)
#endif

using archive_handle = void*;

struct archive_info
{
  size_t entryCount;
  size_t streamCount;
};

extern "C"
{
  BOXLIB_API archive_handle* boxOpenArchive(const char* path);
  BOXLIB_API void boxCloseArchive(archive_handle* handle);
  
  BOXLIB_API archive_info boxGetArchiveInfo(archive_handle* handle);
}