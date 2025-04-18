#pragma once

#include <unordered_map>

#include "hash_map.h"

class DatabaseData
{
public:
  using dat_list = std::unordered_map<std::string, DatFile>;

private:
  dat_list _dats;
  hash_map _hashes;

public:
  //dat_list& dats() { return _dats; }
  const hash_map& hashes() const { return _hashes; }
  const dat_list& dats() const { return _dats; }

  DatFile* addDatFile(const DatFile& dat) { return &(*_dats.insert(std::make_pair(dat.name, dat)).first).second; }
  data_ref addHashData(const HashData& hash)
  {
    return _hashes.add(hash);
  }

  const DatFile* datForName(const std::string& name) const
  {
    auto it = _dats.find(name);
    return it != _dats.end() ? &it->second : nullptr;
  }

  size_t hashesCount() const { return _hashes.size(); }


  size_t aproximateSize() const
  {
    /*size_t sizeForHashes = sizeof(HashData) * _hashes.size();
    size_t sizeForDats = std::accumulate(_dats.begin(), _dats.end(), 0UL, [] (size_t v, const dat_list::value_type& pair) {
      return v + std::accumulate(pair.second.entries.begin(), pair.second.entries.end(), 0UL, [] (size_t v, const decltype(DatFile::entries)::value_type& value) {
        return v + value.first.length() + sizeof(HashData*);
      });
    });

    // round up by a 20% to take into account memory used for data structure internals
    return (sizeForHashes + sizeForDats) * 1.20f;*/
    return 0;
  }
};
