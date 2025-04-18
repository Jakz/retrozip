#pragma once

#include "tbx/hash/hash.h"

struct HashData
{
  struct
  {
    u64 sizeEnabled : 1;
    u64 md5enabled : 1;
    u64 sha1enabled : 1;
    u64 crc32enabled : 1;
    
    u64 size : 60;
  };
  
  hash::crc32_t crc32;
  hash::md5_t md5;
  hash::sha1_t sha1;
  
  HashData() : sizeEnabled(false), md5enabled(false), sha1enabled(false), crc32enabled(false)
  {
    static_assert(sizeof(HashData) == sizeof(u64) + sizeof(hash::crc32_t) + sizeof(hash::sha1_t) + sizeof(hash::md5_t), "");
  }
  
  /*HashData& operator=(const HashData&& other)
  {
    size = other.size;
    crc32 = other.crc32;
    md5 = std::move(other.md5);
    sha1 = std::move(other.sha1);
    return *this;
  }*/

  HashData& operator+=(const HashData& other)
  {
    if (other.crc32enabled)
    {
      crc32 = other.crc32;
      crc32enabled = true;
    }
      
    if (other.md5enabled)
    {
      md5 = other.md5;
      md5enabled = true;
    }

    if (other.sha1enabled)
    {
      sha1 = other.sha1;
      sha1enabled = true;
    }

    if (other.sizeEnabled)
    {
      size = other.size;
      sizeEnabled = true;
    }
    
    return *this;
  }
  
  //TODO: take into account the fact that some fields could not be enabled
  bool operator==(const HashData& other) const
  {
    bool sizeMatch = !sizeEnabled || !other.sizeEnabled || size == other.size;
    bool crc32Match = !crc32enabled || !other.crc32enabled || crc32 == other.crc32;
    bool md5Match = !md5enabled || !other.md5enabled || md5 == other.md5;
    bool sha1Match = !sha1enabled || !other.sha1enabled || sha1 == other.sha1;

    return sizeMatch && crc32Match && md5Match && sha1Match;
  }
  
  bool operator!=(const HashData& other) const
  {
    return !(this->operator==(other));
  }
  
  struct hasher
  {
    size_t operator()(const HashData& o) const
    {
      assert(o.md5enabled || o.sha1enabled || o.crc32enabled);
      
      if (o.crc32enabled)
        return o.crc32;
      if (o.md5enabled)
        return *reinterpret_cast<const size_t*>(o.md5.inner());
      else if (o.sha1enabled)
        return *reinterpret_cast<const size_t*>(o.sha1.inner());
    }
  };
};

#include <unordered_map>

using data_ref = s64;

/* represents a rom entry inside a specific dat */
struct DatRom
{
  std::string name;
  data_ref ref;
};

struct DatGame
{
  std::string name;
  std::vector<DatRom> roms;

  DatGame() = default;
  DatGame(const std::string& name) : name(name) { }

  const DatRom& operator[](size_t index) const { return roms[index]; }
};

struct GameClone
{
  std::vector<data_ref> clones;
};

struct DatFile
{
  std::string name;
  std::string folderName;
  
  std::vector<DatGame> games;
  std::vector<GameClone> clones;

  std::unordered_map<std::string, DatGame*> gameMap;

  DatGame* gameByName(const std::string& name) const
  {
    auto it = gameMap.find(name);
    return it != gameMap.end() ? it->second : nullptr;
  }

  void buildMaps()
  {
    for (auto& game : games)
      gameMap[game.name] = &game;
  }
};
