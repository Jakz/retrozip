#pragma once

#include <string>
#include <map>
#include <unordered_set>
#include <memory>

namespace tags
{
  using tag_name_t = std::string;
  
  class Tag
  {
  protected:
    tag_name_t _name;

  public:
    Tag(const std::string& name) : _name(name) { }
    const tag_name_t& name() const { return _name; }
  };

  class TagPool
  {
  protected:
    std::map<tag_name_t, std::unique_ptr<Tag>> _tags;

  public:
    TagPool() { }

    Tag* registerTag(const std::string& name)
    {
      auto it = _tags.find(name);
      if (it != _tags.end())
        return it->second.get();

      auto tag = std::make_unique<Tag>(name);
      auto result = _tags.insert(std::make_pair(name, std::move(tag)));
      return result.first->second.get();
    }

    auto begin() const { return _tags.begin(); }
    auto end() const { return _tags.end(); }
  };

  struct TagSet
  {
  protected:
    std::unordered_set<const Tag*> _tags;

  public:
    bool contains(const Tag* tag) const { return _tags.contains(tag); }
    bool contains(const tag_name_t& name) const
    {
      for (const auto& tag : _tags)
        if (tag->name() == name)
          return true;
      return false;
    }

    void add(const Tag* tag) { _tags.insert(tag); }
  };
}