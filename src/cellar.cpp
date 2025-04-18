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
#include "data/database.h"
#include "data/hash_map.h"

#include <unordered_set>
#include <unordered_map>
#include <numeric>

#include "cellar/fs/cellar_fs.h"

CellarFS cellar;

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
    
DatabaseData data;
extern void initVFS();


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
  
  std::vector<path> datFiles;
  datFiles = FileSystem::i()->contentsOfFolder("dats");
  
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

    /* preallocate data to be able to get address to Game instances */
    datFile->games.resize(result.games.size());

    /* for each game */
    for (size_t i = 0; i < result.games.size(); ++i)
    {
      const auto& dgame = result.games[i];
      Game& game = datFile->games[i];
      game = Game(dgame.name);

      //const byte* key = entry.hash.sha1.inner();
      //const byte* value = (const byte*) &entry.hash;
      
      //if (!database->contains(std::string((const char*)key)))
      //  database->write(key, sizeof(hash::sha1_t), value, sizeof(HashData));
      
      /* preallocate to have valid size */
      game.roms.resize(dgame.roms.size());

      for (size_t j = 0; j < dgame.roms.size(); ++j)
      {
        /* save hash data into hash repository */
        data_ref ref = data.addHashData(RomRef(&game, j), dgame.roms[j].hash);

        /* this will be mapped later once hash depository have been prepared */
        if (ref != INVALID_DATA_REF)
          game.roms[j] = { dgame.roms[j].name, nullptr };

      }

      /* if game has parent we need to find or generate correct clone */
      if (dgame.parent != INVALID_DATA_REF)
      {
        auto it = std::find_if(
          datFile->clones.begin(),
          datFile->clones.end(),
          [parent = &datFile->games[dgame.parent]](const GameClone& clone) {
            return std::any_of(clone.clones.begin(), clone.clones.end(), [parent](Game* game) { return game == parent; });
        });

        /* add game to existing clone */
        if (it != datFile->clones.end())
          it->clones.push_back(&game);
        else
        {
          assert(dgame.parent <= datFile->games.size());

          /* create new clone */
          GameClone& clone = datFile->clones.emplace_back();
          clone.clones.push_back(&game);
          clone.clones.push_back(&datFile->games[dgame.parent]);
        }
      }
      else
      {
        /* clone with single entry */
        datFile->clones.emplace_back().clones.push_back(&game);
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
      for (const Game* game : it->clones)
        std::cout << "    " << game->name << std::endl;
    }
  }

  /* now it's time to finalize hash repository and map everything 
    rom hash data to its corresponding element in hash repository */
  for (const auto& entry : data.hashes())
  {
    for (auto& ref : entry.roms)
      ref.game->roms[ref.index].hash = &entry;
  }

  auto* lol = &data;

  initVFS();
  
  std::cout << std::dec;
  std::cout << tresult.count << " entries in " << strings::humanReadableSize(tresult.sizeInBytes, true, 2) << std::endl;
  std::cout << data.hashes().size() << " unique entries in " << strings::humanReadableSize(data.hashes().sizeInBytes(), true, 2) << std::endl;
  
  //std::cout << "database memory footprint: " << strings::humanReadableSize(data.aproximateSize(), true, 2) << std::endl;

  //database->shutdown();
  
  CellarFS fs;
  fs.createHandle();
  
  return 0;
}
