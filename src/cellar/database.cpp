#include "database.h"

#include "parsers/parser.h"

#include <set>

using namespace cellar;

void Database::build()
{
  path folder = "dats";
  
  std::vector<path> datFiles;
  datFiles = kernel()->fs()->contentsOfFolder(folder);

  info("scanning for DAT files in {}.. found {} files", folder, datFiles.size());

  parsing::LogiqxParser parser;
  parsing::ParseResult tresult;

  Hasher hasher;

  std::set<std::string> tags;

  for (const auto& dat : datFiles)
  {
    HashData hash = hasher.compute(dat);
    hasher.reset();

    auto result = parser.parse(dat);

    if (result.count == 0)
      result = parsing::ClrMameProParser().parse(dat);

    tresult.sizeInBytes += result.sizeInBytes;
    tresult.count += result.count;

    DatFile* datFile = addDatFile({ dat.filename(), dat.filename() });

    /* preallocate data to be able to get address to Game instances */
    datFile->games.resize(result.games.size());

    /* for each game */
    for (size_t i = 0; i < result.games.size(); ++i)
    {
      const auto& dgame = result.games[i];
      Game& game = datFile->games[i];
      game = Game(dgame.name);

      std::string_view name = dgame.name;
      for (size_t k = 0; k < name.size(); ++k)
      {
        /* search for opening tag */
        if (name[k] == '(')
        {
          auto it = name.find(')', k);

          if (it == std::string_view::npos)
            break;
          else
          {
            std::string tag = std::string(name.substr(k + 1, it - k - 1));
            tags.insert(tag);
            k = it;          
          }
        }
      }

      //const byte* key = entry.hash.sha1.inner();
      //const byte* value = (const byte*) &entry.hash;

      //if (!database->contains(std::string((const char*)key)))
      //  database->write(key, sizeof(hash::sha1_t), value, sizeof(HashData));

      /* preallocate to have valid size */
      game.roms.resize(dgame.roms.size());

      for (size_t j = 0; j < dgame.roms.size(); ++j)
      {
        /* save hash data into hash repository */
        data_ref ref = addHashData(RomRef(&game, j), dgame.roms[j].hash);

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
          verify(dgame.parent <= datFile->games.size(), "a parent game must have an id which is contained in game list");

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

    std::stringstream ss;
    ss
      << std::setw(8) << std::hex << hash.crc32 << " " << std::dec
      << "  " << hash.md5.operator std::string() << " "
      << "  " << hash.sha1.operator std::string() << " "
      << "  " << strings::humanReadableSize(dat.length(), true, 2)
      ;

    debug("{}", dat.filename());
    debug("  {}", ss.str());
    debug("  {} in {} games in {} clones", result.count, strings::humanReadableSize(result.sizeInBytes, true, 2), datFile->clones.size());

    for (const auto& tag : tags)
    {
      debug("  {}", tag);
    }

    /*auto it = std::max_element(
      datFile->clones.begin(),
      datFile->clones.end(),
      [](const GameClone& a, const GameClone& b) {
        return a.clones.size() < b.clones.size();
      });
    if (it != datFile->clones.end())
    {
      for (const Game* game : it->clones)
        std::cout << "    " << game->name << std::endl;
    }*/
  }

  /* now it's time to finalize hash repository and map everything
  rom hash data to its corresponding element in hash repository */
  for (const auto& entry : hashes())
  {
    for (auto& ref : entry.roms)
      ref.game->roms[ref.index].hash = &entry;
  }

  info("{} entries in {}", tresult.count, strings::humanReadableSize(tresult.sizeInBytes, true, 2));
  info("{} unique entries in {}", hashes().size(), strings::humanReadableSize(hashes().sizeInBytes(), true, 2));
}