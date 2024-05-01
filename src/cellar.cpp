//
//  main.cpp
//  romatrix
//
//  Created by Jack on 30/05/18.
//  Copyright © 2018 Jack. All rights reserved.
//



#include <iostream>
#include <iomanip>

#include "tbx/base/path.h"

#include <iostream>

#include "libs/pugixml/pugixml.hpp"
#include "parsers/parser.h"

#include "tbx/base/file_system.h"

#include "data/entry.h"

#include <unordered_set>
#include <unordered_map>
#include <numeric>


#include <cstdio>

#include <io.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "cellar/fs/cellar_fs.h"

CellarFS cellar;

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
    delete [] buffer;
    
    _close(fd);
    
    HashData hashData;
    
    hashData.size = size;
    hashData.crc32 = crc.get();
    hashData.md5 = md5.get();
    hashData.sha1 = sha1.get();
    
    return hashData;
  }
  
  HashData get()
  {
    HashData hashData;
    
    hashData.size = 0;
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



class DatabaseStore
{
public:
  virtual bool init() = 0;
  virtual bool shutdown() = 0;
  
  virtual bool contains(std::string key) = 0;
  virtual bool write(std::string key, std::string value) = 0;
  
  bool write(const byte* key, size_t keyLen, const byte* value, size_t valueLen)
  {
    return write(std::string((const char*)key, keyLen), std::string((const char*)value, valueLen));
  }
};

#include "leveldb/db.h"
class LevelDBDatabase : public DatabaseStore
{
private:
  leveldb::DB* db;
  leveldb::Options options;
  
public:
  bool init() override
  {
    options.create_if_missing = true;
    leveldb::Status status = leveldb::DB::Open(options, R"(database)", &db);
    return status.ok();
  }
  
  bool shutdown() override
  {
    leveldb::Iterator* it = db->NewIterator(leveldb::ReadOptions());
    
    size_t count = 0, sizeInBytes = 0;
    for (it->SeekToFirst(); it->Valid(); it->Next())
    {
      //const hash::sha1_t* key = (const hash::sha1_t*) it->key().data();
      const HashData* data = (const HashData*) it->value().data();
      
      ++count;
      sizeInBytes += data->size;
    }
    
    std::cout << count << " entries in db " << strings::humanReadableSize(sizeInBytes, true, 2) << std::endl;

    
    delete it;
    
    delete db;
    //leveldb::DestroyDB("database", options);

    return true;
  }
  
  bool contains(std::string key) override
  {
    std::string value;
    leveldb::Status status = db->Get(leveldb::ReadOptions(), key, &value);
    return status.ok() && !status.IsNotFound();
  }
  
  bool write(std::string key, std::string value) override
  {
    leveldb::Status status = db->Put(leveldb::WriteOptions(), key, value);
    return status.ok();
  }
};


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

DatabaseData data;
    

using cataloguer_t = std::function<path(const HashData&)>;
    
    
int main(int argc, const char* argv[])
{  
  /*auto files = FileSystem::i()->contentsOfFolder("/Volumes/RAMDisk/input");
  auto root = path("/Volumes/RAMDisk/output");
  
  cataloguer_t cataloguer = [] (const HashData& hd) {
    std::string path = hd.sha1.operator std::string().substr(0,2);
    return path;
  };
  
  for (const auto& file : files)
  {
    Hasher hasher;
    HashData data = hasher.compute(file);
    
    path destination = root + cataloguer(data);
    FileSystem::i()->createFolder(destination);
    destination += file.filename();
    FileSystem::i()->copy(file, destination);
  }

  return 0;*/
  
  auto datFiles = std::vector<path>();// FileSystem::i()->contentsOfFolder("dats");
  
  parsing::LogiqxParser parser;
  
  parsing::ParseResult tresult = {0,0};
  
  //DatabaseStore* database = new LevelDBDatabase();
  //database->init();
  
  Hasher hasher;
  
  for (const auto& dat : datFiles)
  {
    HashData hash = hasher.compute(dat);
    hasher.reset();
    
    auto result = parser.parse(dat);
    
    if (result.count == 0)
      result = parsing::ClrMameProParser().parse(dat);

    tresult.sizeInBytes += result.sizeInBytes;
    tresult.count += result.count;
    
    DatFile* datFile = data.addDatFile({ dat.filename(), dat.filename() });
    
    for (const auto& dgame : result.games)
    {
      //const byte* key = entry.hash.sha1.inner();
      //const byte* value = (const byte*) &entry.hash;
      
      //if (!database->contains(std::string((const char*)key)))
      //  database->write(key, sizeof(hash::sha1_t), value, sizeof(HashData));

      datFile->games.push_back(DatGame(dgame.name));
      auto& game = datFile->games.back();
      auto cref = datFile->games.size() - 1;
      
      for (const auto& rom : dgame.roms)
      {
        data_ref ref = data.addHashData(rom.hash);

        if (ref != INVALID_DATA_REF)
          game.roms.push_back({ rom.name, ref });
      }

      /* if game has parent we need to find or generate correct clone */
      if (dgame.parent != INVALID_DATA_REF)
      {
        auto it = std::find_if(
          datFile->clones.begin(),
          datFile->clones.end(),
          [parent = dgame.parent](const GameClone& clone) {
            return std::any_of(clone.clones.begin(), clone.clones.end(), [parent](data_ref ref) { return ref == parent; });
        });

        if (it != datFile->clones.end())
          it->clones.push_back(cref);
        else
        {
          GameClone clone;
          clone.clones.push_back(cref);
          clone.clones.push_back(dgame.parent);
          datFile->clones.push_back(clone);
        }
      }
      else
      {
        GameClone clone;
        clone.clones.push_back(cref);
        datFile->clones.push_back(clone);
      }
    }

    datFile->buildMaps();

    std::cout
      << dat.filename() << std::endl
      << "  " << std::setw(8) << std::hex << hash.crc32 << " " << std::dec
      << "  " << hash.md5.operator std::string() << " "
      << "  " << hash.sha1.operator std::string() << " "
      << "  " << strings::humanReadableSize(dat.length(), true, 2)
      << std::endl
      << "  " << strings::humanReadableSize(result.sizeInBytes, true, 2) << " in " << result.count << " games in " << datFile->clones.size() << " clones" << std::endl

      ;

    auto it = std::max_element(
      datFile->clones.begin(),
      datFile->clones.end(),
      [](const GameClone& a, const GameClone& b) {
        return a.clones.size() < b.clones.size();
      });
    if (it != datFile->clones.end())
    {
      for (const auto& game : it->clones)
        std::cout << "    " << datFile->games[game].name << std::endl;
    }
  }
  
  std::cout << std::dec;
  std::cout << tresult.count << " entries in " << strings::humanReadableSize(tresult.sizeInBytes, true, 2) << std::endl;
  std::cout << data.hashes().size() << " unique entries in " << strings::humanReadableSize(data.hashes().sizeInBytes(), true, 2) << std::endl;
  
  std::cout << "database memory footprint: " << strings::humanReadableSize(data.aproximateSize(), true, 2) << std::endl;

  //database->shutdown();
  
  CellarFS fs;
  fs.createHandle();
  
  return 0;
}
