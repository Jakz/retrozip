#include "parser.h"

#include "data/entry.h"
#include "libs/pugixml/pugixml.hpp"

#include <vector>

namespace parsing
{
  class SimpleParser
  {
    using char_t = char;
    using callback_t = std::function<void(const std::string&)>;
    
  public:
    class TokenSpec
    {
    public:
      enum class Type { QUOTE, WHITESPACE, SINGLE, NORMAL };
      
    private:
      Type _type;
      char_t _value;
      
    public:
      TokenSpec() { }
      TokenSpec(Type type, char_t value) : _type(type), _value(value) { }
      
      const char_t value() const { return _value; }
      Type type() const { return _type; }
      
      bool operator ==(const Type& type) const { return _type == type; }
    };
    
    
  private:
    size_t _position;
    const std::string& _input;
    
    
    std::string sb;
    callback_t callback;
    
    bool quote;
    std::unordered_map<char, TokenSpec> map;
    TokenSpec defaultToken;
    
    size_t _column, _line;
    
  public:
    SimpleParser(const std::string& input) : SimpleParser(input, [](const std::string&){ }) { }
    
    SimpleParser(const std::string& input, callback_t callback) :
    _input(input), callback(callback), quote(false), defaultToken({TokenSpec::Type::NORMAL, ' '}),
    _column(0), _line(0)
    {
      
    }
    
    void setCallback(const callback_t& callback) { this->callback = callback; }
    
    SimpleParser& addWhiteSpace(const std::initializer_list<char_t>& chars)
    {
      std::for_each(chars.begin(), chars.end(), [this] (char_t c) { map[c] = TokenSpec(TokenSpec::Type::WHITESPACE, c); });
      return *this;
    }
    
    SimpleParser& addSingle(const std::initializer_list<char_t>& chars)
    {
      std::for_each(chars.begin(), chars.end(), [this] (char_t c) { map[c] = TokenSpec(TokenSpec::Type::SINGLE, c); });
      return *this;
    }
    
    SimpleParser& addQuote(const std::initializer_list<char_t>& chars)
    {
      std::for_each(chars.begin(), chars.end(), [this] (char_t c) { map[c] = TokenSpec(TokenSpec::Type::QUOTE, c); });
      return *this;
    }
    
    SimpleParser& addToken(TokenSpec spec)
    {
      map[spec.value()] = spec;
      return *this;
    }
    
  private:
    std::string pop()
    {
      std::string token = sb;
      sb.clear();
      return token;
    }
    
    const TokenSpec& token(char_t c) const
    {
      auto it = map.find(c);
      return it != map.end() ? it->second : defaultToken;
    }
    
    void emit()
    {
      if (!sb.empty())
        callback(pop());
    }
    
  public:
    size_t column() const { return _column; }
    size_t line() const { return _line; }
    
    void parse()
    {
      _column = 0;
      _line = 0;
      _position = 0;
      
      while (_position < _input.length())
      {
        char_t c = _input[_position++];
        
        if (c == '\n')
        {
          ++_line;
          _column = 0;
        }
        else
          ++_column;
        
        const TokenSpec& tkn = token(c);
        
        if (tkn == TokenSpec::Type::WHITESPACE && !quote)
        {
          emit();
          
          continue;
        }
        else if (tkn == TokenSpec::Type::QUOTE)
        {
          if (quote)
          {
            emit();
            quote = false;
          }
          else
            quote = true;
        }
        else if (!quote && tkn == TokenSpec::Type::SINGLE)
        {
          emit();
          callback(std::string(1, c));
        }
        else
          sb += c;
      }
      
      emit();
    }
  };
  
  class SimpleTreeParser
  {
  public:
    using token_t = std::string;
    using char_t = char;
    using callback_pair_t = std::function<void(const token_t&, const std::string&)>;
    using callback_scope_t = std::function<void(const token_t&, bool)>;
    
  private:
    SimpleParser& parser;
    token_t scope[2];
    std::vector<token_t> tokens;
    bool partial;
    
    callback_pair_t callbackPair;
    callback_scope_t callbackScope;
    
  public:
    SimpleTreeParser(SimpleParser& parser, const callback_pair_t& callbackPair, const callback_scope_t& callbackScope) :
    parser(parser), callbackPair(callbackPair), callbackScope(callbackScope), partial(false)
    {
      parser.setCallback([this](const token_t& tkn) { this->token(tkn); });
    }
    
    void setScope(const token_t& start, const token_t& end)
    {
      scope[0] = start;
      scope[1] = end;
    }
    
  private:
    bool isStartScope(const token_t& token) const { return token == scope[0]; }
    bool isEndScope(const token_t& token) const { return token == scope[1]; }
    bool isScope(const token_t& token) const { return isStartScope(token) || isEndScope(token); }
    
    void token(const token_t& tkn)
    {
      bool wasPartial = partial;
      
      if (!partial)
        partial = true;
      
      if (!isScope(tkn))
        tokens.push_back(tkn);
      
      if (isStartScope(tkn))
      {
        partial = false;
        callbackScope(tokens.back(), false);
      }
      else if (isEndScope(tkn))
      {
        partial = false;
        callbackScope(tokens.back(), true),
        tokens.pop_back();
      }
      else
      {
        if (wasPartial)
        {
          token_t value = tokens.back();
          tokens.pop_back();
          token_t key = tokens.back();
          tokens.pop_back();
          
          callbackPair(key, value);
          partial = false;
        }
      }
    }
    
  };
  
  ParseResult ClrMameProParser::parse(const path& path)
  {
    file_handle handle = file_handle(path, file_mode::READING);
    std::string contents = handle.toString();
    
    SimpleParser parser = SimpleParser(contents);
    parser.addSingle({'(',')'}).addQuote({'\"'}).addWhiteSpace({' ','\t','\r','\n'});
    
    SimpleTreeParser treeParser = SimpleTreeParser(
      parser,
      [this] (const std::string& s1, const std::string& s2) { this->pair(s1,s2); },
      [this] (const std::string& s1, bool isEnd) { this->scope(s1, isEnd); });
    treeParser.setScope("(", ")");
    
    result.games.clear();
    result.count = 0;
    result.sizeInBytes = 0;
    
    started = false;
    _scope = Scope::Root;
    
    parser.parse();
    
    return result;
  }
  
  void ClrMameProParser::pair(const std::string& k, const std::string& v)
  {
    if (!started)
      return;

    if (k == "name")
    {
      if (_scope == Scope::Rom)
        _rom.name = v;
      else if (_scope == Scope::Game)
        _game.name = v;
    }
    else if (k == "size")
    {
      _rom.hash.size = (size_t)std::strtoull(v.c_str(), nullptr, 10);
      _rom.hash.sizeEnabled = true;
    }
    else if (k == "crc")
    {
      _rom.hash.crc32 = (hash::crc32_t)std::strtoull(v.c_str(), nullptr, 16);
      _rom.hash.crc32enabled = true;
    }
    else if (k == "md5")
    {
      _rom.hash.md5 = strings::toByteArray(v);
      _rom.hash.md5enabled = true;
    }
    else if (k == "sha1")
    {
      _rom.hash.sha1 = strings::toByteArray(v);
      _rom.hash.sha1enabled = true;
    }
  }
  
  void ClrMameProParser::scope(const std::string& k, bool isEnd)
  {
    if (k == "rom")
    {
      if (isEnd)
      {
        result.count += 1;
        result.sizeInBytes += _rom.hash.size;
        
        _game.roms.push_back(_rom);
        _scope = Scope::Game;
      }
      else
      {
        _scope = Scope::Rom;
        _rom = ParseRom();
      }
    }
    else if (k == "game")
    {
      if (isEnd)
      {
        result.games.push_back(_game);
        _scope = Scope::Root;
      }
      else
      {
        _scope = Scope::Game;
        _game = ParseGame();
      }
    }
    else if (isEnd && k == "clrmamepro")
    {
      started = true;
    }
  }
}
