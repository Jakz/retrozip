#include "ui.h"

#include "nana/gui.hpp"
#include "nana/gui/widgets/label.hpp"
#include "nana/gui/widgets/treebox.hpp"
#include "nana/gui/widgets/toolbar.hpp"
#include "nana/gui/widgets/textbox.hpp"
#include "nana/gui/place.hpp"

#include "cellar/fs/cellar_fs.h"
#include "cellar/database.h"
#include "data/meta.h"

using namespace cellar;

UserInterface::UserInterface(Kernel* kernel, const std::string& name) :
  KernelModule(kernel, name),
  _form(), _console(_form)

{ }

void UserInterface::init() 
{
  nana::place layout(_form);

  nana::toolbar toolbar(_form);

  _console.typeface(nana::paint::font("Consolas", 10.0f));
  _console.bgcolor(nana::color(0, 0, 30));
  _console.fgcolor(nana::color(255, 255, 255));
  //_console.editable(false);
  _console.multi_lines(true);
  _console.events().key_char.connect([&](const nana::arg_keyboard& arg) {
    if (arg.ctrl && (arg.key == 0x03))
      return;
    arg.ignore = true;
  });

  auto toggleFuse = toolbar.append("foobar");
  toggleFuse.answerer([&](auto&) {
    if (kernel()->vfs()->isRunning())
      kernel()->vfs()->stop();
    else
      kernel()->vfs()->start();
  });

  toolbar.separate();
  toolbar.append("baz");

  toolbar.textout(0, true);
  toolbar.textout(2, true);

  nana::treebox tree(_form);
  layout.div("vertical <toolbar weight=28> <tree> <console weight=200>");
  layout["toolbar"] << toolbar;
  layout["tree"] << tree;
  layout["console"] << _console;

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
  _form.move(nana::rectangle((screen.width - size.width) / 2, (screen.height - size.height) / 2, size.width, size.height));

  _form.show();
  nana::exec();
}

void UserInterface::appendConsoleMessage(const std::string& message)
{
  _console.append(message, true);
  _console.append("\n", true);
}