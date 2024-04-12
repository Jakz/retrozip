#include "parser.h"

#include "data/entry.h"
#include "libs/pugixml/pugixml.hpp"

namespace parsing
{
  enum class Status { BADDUMP, NODUMP, GOOD, VERIFIED };
  
  ParseResult LogiqxParser::parse(const path& path)
  {
    pugi::xml_document doc;
    pugi::xml_parse_result xmlResult = doc.load_file(path.c_str());
    
    ParseResult result = { 0, 0 };

    /* map name/id to position in array */
    std::unordered_map<std::string, size_t> gameMap;
    /* map child index to name/id of parent */
    std::unordered_map<size_t, std::string> cloneMap;
    
    if (xmlResult)
    {
      const auto& games = doc.child("datafile").children("game");
      
      for (pugi::xml_node xgame : games)
      {
        ParseGame game;
        game.name = xgame.attribute("name").as_string();
        
        const auto& roms = xgame.children("rom");
        
        for (pugi::xml_node xrom : roms)
        {
          ParseRom rom;
          
          const std::string statusAttribute = xrom.attribute("status").as_string();
          Status status = Status::GOOD;

          if (statusAttribute == "nodump") status = Status::NODUMP;
          else if (statusAttribute == "baddump") status = Status::BADDUMP;
          else if (statusAttribute == "verified") status = Status::VERIFIED;
          
          if (status != Status::VERIFIED && status != Status::GOOD)
            continue;
          
          rom.name = xrom.attribute("name").as_string();
          rom.hash.size = (size_t)std::strtoull(xrom.attribute("size").as_string(), nullptr, 10);
          rom.hash.sizeEnabled = true;
          
          const auto crc = xrom.attribute("crc"), md5 = xrom.attribute("md5"), sha1 = xrom.attribute("sha1");
          
          if (!crc.empty())
          {
            rom.hash.crc32 = (hash::crc32_t)std::strtoull(xrom.attribute("crc").as_string(), nullptr, 16);
            rom.hash.crc32enabled = true;
          }
          
          if (!md5.empty())
          {
            rom.hash.md5 = strings::toByteArray(xrom.attribute("md5").as_string());
            rom.hash.md5enabled = true;
          }
          
          if (!sha1.empty())
          {
            rom.hash.sha1 = strings::toByteArray(xrom.attribute("sha1").as_string());
            rom.hash.sha1enabled = true;
          }
          
          assert(rom.hash.md5enabled || rom.hash.sha1enabled);
                 
          ++result.count;
          result.sizeInBytes += rom.hash.size;

          game.roms.push_back(rom);
        }

        /* map id to game index */
        if (!xgame.attribute("id").empty())
          gameMap[xgame.attribute("id").as_string()] = result.games.size();

        gameMap[game.name] = result.games.size();

        if (!xgame.attribute("cloneofid").empty())
          cloneMap[result.games.size()] = xgame.attribute("cloneofid").as_string();

        if (!xgame.attribute("cloneof").empty())
          cloneMap[result.games.size()] = xgame.attribute("cloneof").as_string();

        result.games.push_back(game);
      }
    }

    /* solve clones */
    for (const auto& entry : cloneMap)
    {
      auto parent = gameMap.find(entry.second);
      if (parent != gameMap.end())
        result.games[entry.first].parent = parent->second;
    }
  
    return result;
  }
}
