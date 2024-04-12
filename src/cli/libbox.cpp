#include "cli/box.h"

#include "box/archive.h"
#include "tbx/streams/file_data_source.h"

struct ArchiveHandle
{
  archive_handle* handle;

  std::string path;
  Archive archive;
};

std::vector<std::unique_ptr<ArchiveHandle>> archives;

archive_handle* boxOpenArchive(const char* path)
{
  ArchiveHandle* handle = new ArchiveHandle();

  handle->path = path;
  handle->handle = (archive_handle*)handle;
  handle->archive = Archive();

  auto source = file_data_source(path);
  handle->archive.read(source);

  archives.emplace_back(std::unique_ptr<ArchiveHandle>(handle));

  return handle->handle;
}

void boxCloseArchive(archive_handle* handle)
{
  auto it = std::find_if(archives.begin(), archives.end(), [handle](const std::unique_ptr<ArchiveHandle>& ptr) {
    return ptr->handle == handle;
  });

  if (it != archives.end())
    archives.erase(it);
}

ArchiveInfo boxGetArchiveInfo(archive_handle* handle)
{
  ArchiveInfo info;

  info.entryCount = ((ArchiveHandle*)handle)->archive.entries().size();
  info.streamCount = ((ArchiveHandle*)handle)->archive.streams().size();

  return info;
}