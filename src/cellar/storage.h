#pragma once

#include "data/entry.h"
#include "kernel.h"

namespace cellar
{
  class StorageFile
  {
  protected:
    path _path;

  public:
    StorageFile() { }
    StorageFile(const path& path) : _path(path) { }

    auto path() const { return _path; }
  };

  /* this class manages the mapping between recognized files and how they're stored on physical disk */
  class Storage : public KernelComponent
  {
  protected:
    std::unordered_map<hash::sha1_t, StorageFile, hash::sha1_t::hasher> _files;

  public:
    Storage(Kernel* kernel) : KernelComponent(kernel) { }

    void save() const;

    void map(const hash::sha1_t& sha1, const path& path)
    {
      _files[sha1] = StorageFile(path);
    }

    bool isOwned(const DatRom* rom) const;
  };
}
