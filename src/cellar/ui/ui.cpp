#include "ui.h"

#include "nana/gui.hpp"
#include "nana/gui/widgets/label.hpp"
#include "nana/gui/widgets/treebox.hpp"
#include "nana/gui/widgets/toolbar.hpp"
#include "nana/gui/place.hpp"

#include "cellar/database.h"
#include "data/meta.h"

using namespace cellar;

void UserInterface::init()
{
  nana::form form;
  nana::place layout(form);

  nana::toolbar toolbar(form);

  toolbar.append("foobar");
  toolbar.separate();
  toolbar.append("baz");

  toolbar.textout(0, true);
  toolbar.textout(2, true);

  nana::treebox tree(form);
  layout.div("vertical <toolbar weight=28> <tree>");
  layout["toolbar"] << toolbar;
  layout["tree"] << tree;

  std::unordered_map<const meta::Company*, std::vector<const meta::System*>> systemsByCompany;
  for (const auto& system : meta::Repository::i()->systems())
    systemsByCompany[system.company()].push_back(&system);

  /* sort each system by company alphabetically */
  for (auto& [company, systems] : systemsByCompany)
  {
    std::sort(systems.begin(), systems.end(), [](const auto& a, const auto& b) {
      return a->name() < b->name();
    });
  }


  tree.insert("systems", "Systems");

  for (const auto& [company, systems] : systemsByCompany)
  {
    auto companyItem = tree.insert("systems/" + company->ident(), company->name());
    for (const auto& system : systems)
    {
      auto systemItem = tree.insert("systems/" + company->ident() + "/" + system->ident(), system->longName());

      auto icon = nana::paint::image(R"(C:\Users\Jack\Documents\dev\retrozip\projects\msvc2017\icons\)" + system->shortName() + ".bmp");

      if (!icon.empty())
      {
        auto& iconSet = tree.icon("icons/" + system->shortName());
        iconSet.normal = icon;
        systemItem.icon("icons/" + system->shortName());
      }


      for (const auto& dat : kernel()->db()->dats())
      {
        if (dat.second.system == system)
        {
          tree.insert(systemItem, systemItem.key() + "/" + dat.second.name, dat.second.name);

          for (const auto& game : dat.second.games)
          {
            if (game.hasSingleRom())
            {
              /* directly add rom instead that nest it into game */
              auto romItem = tree.insert(systemItem, systemItem.key() + "/" + dat.second.name + "/" + game.name, game[0].name);
            }
            else
            {
              auto gameItem = tree.insert(systemItem, systemItem.key() + "/" + dat.second.name + "/" + game.name, game.name);

              for (const auto& rom : game.roms)
              {
                tree.insert(gameItem, gameItem.key() + "/" + rom.name, rom.name);
              }
            }
          }
        }
      }
    }
  }

  /*
  tree.insert("Consoles", "Systems");
  tree.insert("Consoles/Nintendo", "Nintendo");
  tree.insert("Consoles/Sega", "Sega");

  auto item = tree.insert("Consoles/Nintendo/NES", "Nintendo NES");
  tree.insert("Consoles/Nintendo/SNES", "Nintento SNES");
  tree.insert("Consoles/Sega/MegaDrive", "MegaDrive");

  auto& iconSet = tree.icon("test");
  iconSet.normal = nes_icon;


  //auto id = tree.icon("icons/nes_icon.png");
  item.icon("test");*/
  
  layout.collocate();

  auto size = nana::size(1280, 800);

  auto screen = nana::screen::primary_monitor_size();
  form.move(nana::rectangle((screen.width - size.width) / 2, (screen.height - size.height) / 2, size.width, size.height));

  form.show();
  nana::exec();
}