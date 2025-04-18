#pragma once

#include <unordered_map>
#include <vector>

#include "entry.h"
#include "tbx/hash/hash.h"

using data_ref = s64;
static constexpr data_ref INVALID_DATA_REF = -1;

struct RomData
{
  HashData hash;
  size_t references;

  RomData() : references(0UL)
  {

  }

  size_t size() const { return hash.size; }

  RomData& operator+=(const HashData& hash)
  {
    this->hash += hash;
    ++this->references;

    return *this;
  }
};

struct hash_map
{
private:
  std::vector<RomData> _data;
  std::unordered_map<hash::crc32_t, data_ref> _crc32map;
  std::unordered_map<hash::md5_t, data_ref, hash::md5_t::hasher> _md5map;
  std::unordered_map<hash::sha1_t, data_ref, hash::sha1_t::hasher> _sha1map;

public:
  hash_map()
  {

  }

  const RomData& operator[](size_t index) const { return _data[index]; }

  bool isCollision(const HashData& entry, data_ref ref)
  {
    return entry != _data[ref].hash;
  }

  data_ref add(const HashData& entry)
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
      _data.push_back(RomData());
      ref = _data.size() - 1;
    }

    if (ref != INVALID_DATA_REF)
    {
      _data[ref] += entry;

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
};