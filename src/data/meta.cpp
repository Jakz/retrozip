#include "meta.h"

using namespace meta;

Repository* Repository::i()
{
  static Repository instance;
  return &instance;
}

Company* Repository::company(const ident_t& ident)
{
  for (auto& company : _companies)
  {
    if (company.ident() == ident)
      return &company;
  }
  return nullptr;
}


Repository::Repository()
{
  _companies = {
    Company("nintendo", "Nintendo"),
    Company("sega", "Sega"),
  };
  
  _systems = {
    System("nes", company("nintendo"), "NES", "Nintendo Entertainment System"),
    System("snes", company("nintendo"), "SNES", "Super Nintendo Entertainment System"),

    System("md", company("sega"), "MD", "Sega MegaDrive/Genesis"),
  };
}