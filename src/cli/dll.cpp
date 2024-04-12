#include "cli/box.h"

#include "box/archive.h"
#include "tbx/streams/file_data_source.h"
#include "box/archive_builder.h"

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

void boxFillArchiveInfo(archive_handle* handle, ArchiveInfo* info)
{
  info->entryCount = ((ArchiveHandle*)handle)->archive.entries().size();
  info->streamCount = ((ArchiveHandle*)handle)->archive.streams().size();
}

ArchiveInfo boxGetArchiveInfo(archive_handle* handle)
{
  ArchiveInfo info;
  boxFillArchiveInfo(handle, &info);
  return info;
}

void boxFillEntryInfo(archive_handle* handle, size_t entryIndex, EntryInfo* info)
{
  Archive& archive = ((ArchiveHandle*)handle)->archive;

  if (entryIndex < archive.entries().size())
  {
    const auto& entry = archive.entries()[entryIndex];
    
    info->name = entry.name().c_str();

    info->size = entry.binary().digest.size;
    info->filteredSize = entry.binary().filteredSize;

    info->crc32 = entry.binary().digest.crc32;
    info->md5 = entry.binary().digest.md5.inner();
    info->sha1 = entry.binary().digest.sha1.inner();
  }
}

BoxResult boxExtractEntryFromArchive(archive_handle* handle, size_t entryIndex, const char* destination)
{
  Archive& archive = ((ArchiveHandle*)handle)->archive;

  if (entryIndex >= archive.entries().size())
    return BoxResult::EntryIsOutsideRange;

  const auto& entry = archive.entries()[entryIndex];

  {
    auto source = file_data_source(((const ArchiveHandle*)handle)->path);

    ArchiveReadHandle archiveHandle = ArchiveReadHandle(source, archive, entry);
    auto* entrySource = archiveHandle.source(true);

    file_data_sink sink(destination);

    passthrough_pipe pipe(entrySource, &sink, BufferSizePolicy(BufferSizePolicy::Mode::AS_LARGE_AS_SOURCE));
    pipe.process(entry.binary().digest.size);
  }

  return BoxResult::Success;
}