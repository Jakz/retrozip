#include "vfs.h"

#include "data/hash_map.h"

#include <iostream>

using namespace cellar::vfs;

size_t VirtualFile::write(const char* buf, size_t length, FUSE_OFF_T offset)
{
  size_t requiredSize = offset + length;
  if (_content.size() < requiredSize)
    _content.resize(requiredSize);

  memcpy(_content.data() + offset, buf, length);

  stbuf.st_size = std::max(_content.size(), (size_t)stbuf.st_size);
  return length;
}

size_t VirtualFile::read(char* buf, size_t length, FUSE_OFF_T offset)
{
  if (offset < _content.size())
  {
    if (offset + length > _content.size())
      length = _content.size() - offset;
    memcpy(buf, _content.data() + offset, length);
  }
  else
    length = 0;

  return length;
}

VirtualFile* VirtualDirectory::get(const fs_path& filename)
{
  auto it = _flatMapping.find(filename);
  VirtualEntry* entry = nullptr;

  if (it != _flatMapping.end())
    entry = it->second;

  if (entry && entry->isFile())
    return static_cast<VirtualFile*>(entry);
  else
    return nullptr;
}


void VirtualDirectory::print(size_t indent)
{
  std::string indentation(indent, ' ');
  std::cout << indentation << _path << std::endl;

  for (size_t i = 0; i < count(); ++i)
  {
    auto* entry = get(i);

    if (entry->isFile())
      std::cout << std::string(indent + 2, ' ') << entry->filename() << std::endl;
    else
      ((VirtualDirectory*)entry)->print(indent + 2);
  }
}

void VirtualDirectory::add(VirtualEntry* entry)
{
  _content.push_back(entry);
  _flatMapping[entry->filename()] = entry;
}

bool VirtualDirectory::remove(VirtualEntry* entry)
{
  if (entry)
  {
    auto it = std::find(_content.begin(), _content.end(), entry);
    if (it != _content.end())
    {
      _flatMapping.erase(entry->filename());
      _content.erase(it);
      return true;
    }
  }

  return false;
}


VirtualFileSystem::VirtualFileSystem(Kernel* kernel, const std::string& name) : KernelModule(kernel, name)
{
  _root.reset(new VirtualDirectory("/"));

  _toSortFolder = new VirtualDirectory("/ToSort");
  _root->add(_toSortFolder);

  _flatMapping["/"] = _root.get();
  _flatMapping["/ToSort"] = _toSortFolder;

  initStat(&_defaultDirectoryStat, true, true);
}

void VirtualFileSystem::generateFoldersForDATs()
{
  info("Generating folders for DATs...");
  
  VirtualDirectory* dats = new VirtualDirectory("/Dats");
  _root->add(dats);
  _flatMapping["/Dats"] = dats;

  /*for (const auto& dat : data.dats())
  {
    path name = dat.second.name;
    path folderName = name.filenameWithoutExtension();
    path folderPath = path("/Dats/") + folderName;
    auto* folder = new VirtualDirectory(folderPath);
    _flatMapping[folderPath] = folder;
    dats->add(folder);

    /*for (const DatGame& game : dat.second.games)
    {
      if (!game.roms.empty())
      {
        const DatRom& rom = game[0];

        VirtualFile* file = new VirtualFile(folderPath + rom.name);
        initStat(file, true);
        file->setSize(data.hashes()[rom.ref].size());

        folder->add(file);
      }
    }
  }*/
}

VirtualDirectory* VirtualFileSystem::findDirectory(const path& path)
{
  auto it = _flatMapping.find(path);
  return it != _flatMapping.end() ? it->second : nullptr;
}

void VirtualFileSystem::initStat(FUSE_STAT* stbuf, bool dir = false, bool readonly = false)
{
  memset(stbuf, 0, sizeof(FUSE_STAT));
  stbuf->st_nlink = 1;

  stbuf->st_uid = 1000;
  stbuf->st_gid = 1000;

  auto now = std::time(nullptr);
  stbuf->st_mtim.tv_sec = now;
  stbuf->st_mtim.tv_nsec = 0;
  stbuf->st_atim = stbuf->st_mtim;
  stbuf->st_ctim = stbuf->st_mtim;

  if (dir)
  {
    stbuf->st_mode = S_IFDIR;
    stbuf->st_mode |= 0111;
  }
  else
    stbuf->st_mode = S_IFREG;

  if (readonly)
    stbuf->st_mode |= 0444;
  else
    stbuf->st_mode |= 0666;
}

#include "fuse_backend.h"
#include "cellar/database.h"
#include "cellar/storage.h"

void VirtualFileSystem::mount()
{
  FuseBackend fuse;
  generateFoldersForDATs();
  fuse.mount(this);
}

bool VirtualFileSystem::filesReadyToBeSorted(VirtualFile* file)
{
  Hasher hasher;
  auto hash = hasher.compute(file->_content.data(), file->_content.size());

  auto rom = kernel()->db()->hashes().find(hash);

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

    return true;
  }

  return false;
}