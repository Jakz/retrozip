#pragma once

#include <unordered_map>
#include <vector>

#include "entry.h"
#include "tbx/hash/hash.h"

using data_ref = s64;
static constexpr data_ref INVALID_DATA_REF = -1;

struct RomRef
{
  Game* game;
  size_t index;

  RomRef(Game* game, size_t index) 
    : game(game), index(index) { }

  DatRom* operator*() { return &game->roms[index]; }
  const DatRom* operator*() const { return &game->roms[index]; }
  DatRom* operator->() { return &game->roms[index]; }
  const DatRom* operator->() const { return &game->roms[index]; }
};

struct RomHashData
{
  HashData hash;
  std::vector<RomRef> roms;
  size_t references;

  RomHashData() : references(0UL)
  {

  }

  size_t size() const { return hash.size; }

  RomHashData& operator+=(const HashData& hash)
  {
    this->hash += hash;
    ++this->references;

    return *this;
  }
};

struct hash_map
{
private:
  std::vector<RomHashData> _data;
  std::unordered_map<hash::crc32_t, data_ref> _crc32map;
  std::unordered_map<hash::md5_t, data_ref, hash::md5_t::hasher> _md5map;
  std::unordered_map<hash::sha1_t, data_ref, hash::sha1_t::hasher> _sha1map;

public:
  hash_map()
  {

  }

  const RomHashData& operator[](size_t index) const { return _data[index]; }

  bool isCollision(const HashData& entry, data_ref ref)
  {
    return entry != _data[ref].hash;
  }

  const RomHashData* find(const HashData& entry) const
  {
    auto it = _sha1map.find(entry.sha1);
    if (it != _sha1map.end())
      return &_data[it->second];

    return nullptr;
  }

  data_ref add(RomRef rom, const HashData& entry)
  {
    data_ref ref = INVALID_DATA_REF;

    /* first we search is an entry with same crc32 is found */
    if (entry.crc32enabled)
    {
      auto it = _crc32map.find(entry.crc32);

      if (it == _crc32map.end())
        ;
      /* we expect that hash data is matching otherwise it's a problem */
      else if (!isCollision(entry, it->second))
      {
        ref = it->second;
      }
      else
        return INVALID_DATA_REF;
    }

    /* we search is an entry with same md5 is found */
    if (entry.md5enabled)
    {
      auto it = _md5map.find(entry.md5);

      if (it == _md5map.end())
        ;
      /* we expect that hash data is matching otherwise it's a problem */
      else if (!isCollision(entry, it->second))
      {
        ref = it->second;
      }
      else
        return INVALID_DATA_REF;
    }

    /* we search is an entry with same sha1 is found */
    if (entry.sha1enabled)
    {
      auto it = _sha1map.find(entry.sha1);

      if (it == _sha1map.end())
        ;
      /* we expect that hash data is matching otherwise it's a problem */
      else if (!isCollision(entry, it->second))
      {
        ref = it->second;
      }
      else
        return INVALID_DATA_REF;
    }

    if (ref == INVALID_DATA_REF)
    {
      _data.push_back(RomHashData());
      ref = _data.size() - 1;
    }

    if (ref != INVALID_DATA_REF)
    {
      _data[ref] += entry;
      _data[ref].roms.push_back(rom);

      if (entry.crc32enabled)
        _crc32map[entry.crc32] = ref;

      if (entry.md5enabled)
        _md5map[entry.md5] = ref;

      if (entry.sha1enabled)
        _sha1map[entry.sha1] = ref;

      return ref;
    }

    return INVALID_DATA_REF;
  }

  size_t size() const { return _data.size(); }

  size_t sizeInBytes() const
  {
    size_t total = 0;
    for (const auto& entry : _data)
      total += entry.size();
    return total;
  }

  auto begin() const { return _data.begin(); }
  auto end() const { return _data.end(); }
};


#include <cstdio>

#include <io.h>
#include <sys/stat.h>
#include <fcntl.h>

class Hasher
{
  using fd_t = int;

  hash::crc32_digester crc;
  hash::sha1_digester sha1;
  hash::md5_digester md5;

  size_t sizeOfFile(fd_t fd)
  {
    struct stat statbuf;
    fstat(fd, &statbuf);
    return statbuf.st_size;
  }

public:

  HashData compute(const path& path)
  {
    fd_t fd = _open(path.c_str(), O_RDONLY);
    size_t size = sizeOfFile(fd);

    byte* buffer = new byte[size];
    _read(fd, buffer, size);

    //byte* buffer = reinterpret_cast<byte*>(mmap(nullptr, size, PROT_READ, MAP_SHARED, fd, 0));
    crc.update(buffer, size);
    md5.update(buffer, size);
    sha1.update(buffer, size);

    //munmap(buffer, size);
    delete[] buffer;

    _close(fd);

    return get(size);
  }

  HashData compute(const void* data, size_t length)
  {
    crc.update(data, length);
    md5.update(data, length);
    sha1.update(data, length);
    return get(length);
  }

  HashData get(size_t length = 0)
  {
    HashData hashData;

    hashData.size = length;
    hashData.crc32 = crc.get();
    hashData.md5 = md5.get();
    hashData.sha1 = sha1.get();

    return hashData;
  }

  void update(const void* data, size_t length)
  {
    crc.update(data, length);
    md5.update(data, length);
    sha1.update(data, length);
  }

  void reset()
  {
    crc.reset();
    md5.reset();
    sha1.reset();
  }
};


