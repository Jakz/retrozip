#include "tbx/base/path.h"

#include "data/entry.h"

#include <vector>

namespace parsing
{  
  struct ParseRom
  {
    HashData hash;
    std::string name;
  };

  struct ParseGame
  {
    std::string name;
    std::vector<ParseRom> roms;
    data_ref parent;

    ParseGame() : parent(-1) { }
  };
  
  struct ParseResult
  {
    size_t count;
    size_t sizeInBytes;
    std::vector<ParseGame> games;
  };

  class Parser
  {
  public:
    virtual ParseResult parse(const path& path) = 0;
  };
  
  class LogiqxParser : public Parser
  {
  public:
    ParseResult parse(const path& path) override;
  };
  
  class ClrMameProParser : public Parser
  {
  private:
    enum class Scope { Root, Game, Rom };

    bool started;
    Scope _scope;
    
    ParseRom _rom;
    ParseGame _game;
      
    ParseResult result;
        
    void pair(const std::string& key, const std::string& value);
    void scope(const std::string& tkn, bool isEnd);
    
  public:
    ParseResult parse(const path& path) override;
  };
};
