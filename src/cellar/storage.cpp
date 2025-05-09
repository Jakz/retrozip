#include "storage.h"

#include "data/hash_map.h"
#include "tbx/hash/hash.h"

#include "cellar/fs/cellar_fs.h"

#define RYML_SINGLE_HDR_DEFINE_NOW
#include "libs/rapidyaml.hpp"

using namespace cellar;

void Storage::save() const
{
  path vaultPath = path("vault/vault.yml");

  ryml::Tree tree;
  ryml::NodeRef node = tree.rootref();

  node |= ryml::MAP;

  for (const auto& file : _files)
  {
    std::string hash = std::string(file.first.literal());
    auto child = node.append_child();
    child << ryml::key(hash);
    child << file.second.path().c_str();
  }

  std::string output = ryml::emitrs_yaml<std::string>(tree);

  FILE* out = fopen(vaultPath.c_str(), "wb+");
  fwrite(output.c_str(), output.size(), 1, out);
  fclose(out);
}

bool Storage::isOwned(const DatRom* rom) const
{
  // TODO: check if sha1 is actually used for the rom
  auto it = _files.find(rom->hash->sha1());
  return it != _files.end();
}

#include "tbx/formats/minizip/zip.h"
#include "box/archive_builder.h"

void Storage::consolidate(const RomHashData* rom, vfs::VirtualFile* file)
{

  /* organize by sha1 */
  verify(rom->hash.sha1enabled, "only sha1 roms are supported for now");

  path base = path("vault");
  base = (base + rom->hash.sha1.literal().substr(0, 2)) + (rom->hash.sha1.literal() + ".bin");

  info("organizing {} -> {}", file->filename(), base);

  kernel()->fs()->createFolder(base.parent(), true);

  auto out = fopen(base.c_str(), "wb+");
  if (out)
  {
    size_t written = fwrite(file->_content.data(), file->_content.size(), 1, out);
    fclose(out);

    kernel()->storage()->map(rom->hash.sha1, base);
    kernel()->storage()->save();
  }

  zipFile zip = zipOpen(base.withExtension("zip").c_str(), APPEND_STATUS_CREATE);
  if (zip)
  {
    zip_fileinfo zi = {};
    zipOpenNewFileInZip(zip, base.filename().c_str(), &zi, nullptr, 0, nullptr, 0, nullptr, Z_DEFLATED, Z_DEFAULT_COMPRESSION);
    zipWriteInFileInZip(zip, file->_content.data(), file->_content.size());

    zipCloseFileInZip(zip);
    zipClose(zip, nullptr);
  }

  ArchiveBuilder builder(CachePolicy(CachePolicy::Mode::NEVER, 0), MB128, MB128);
  memory_buffer* buffer = new memory_buffer(file->_content.data(), file->_content.size(), false);
  data_source_vector sources;
  sources.emplace_back(base.filename(), buffer);
  Archive archive = builder.buildSingleStreamSolidArchive(sources);
  memory_buffer sink;
  archive.options().bufferSize = MB1;
  archive.write(sink);
  sink.serialize(file_handle(base.withExtension("box"), file_mode::WRITING));
  
}