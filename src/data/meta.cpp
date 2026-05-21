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

System* Repository::system(const ident_t& ident)
{
  for (auto& system : _systems)
  {
    if (system.ident() == ident)
      return &system;
  }
  return nullptr;
}


Repository::Repository()
{
  _companies = {
    Company("nintendo", "Nintendo"),
    Company("sega", "Sega"),
    Company("commodore", "Commodore"),
	Company("arcade", "Arcade"),
  };
  
  _systems = {
    System("nes", SystemType::Console, company("nintendo"), "NES", "Nintendo Entertainment System"),
    System("snes", SystemType::Console, company("nintendo"), "SNES", "Super Nintendo Entertainment System"),
    System("gba", SystemType::Handheld, company("nintendo"), "GBA", "Game Boy Advance"),
    System("ns", SystemType::HybridConsole, company("nintendo"), "NS", "Nintendo Switch"),

    System("md", SystemType::Console, company("sega"), "MD", "Sega MegaDrive/Genesis"),

    System("a500", SystemType::Computer, company("commodore"), "A500", "Amiga 500"),

	System("arcade", SystemType::Arcade, company("arcade"), "Arcade", "Arcade")
  };
}