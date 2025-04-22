#include "ui.h"

#include "nana/gui.hpp"
#include "nana/gui/widgets/label.hpp"
#include "nana/gui/widgets/treebox.hpp"

using namespace cellar;

void UserInterface::init()
{
  nana::form form;

  nana::label label(form, nana::rectangle(0, 0, 100, 20));
  label.caption("Hello Nana");

  nana::paint::image nes_icon = nana::paint::image(R"(C:\Users\Jack\Documents\dev\retrozip\projects\msvc2017\icons\nes.bmp)");

  assert(!nes_icon.empty());

  nana::treebox tree(form, nana::rectangle(0, 20, 200, 200));
  tree.insert("Consoles", "Systems");
  tree.insert("Consoles/Nintendo", "Nintendo");
  tree.insert("Consoles/Sega", "Sega");

  auto item = tree.insert("Consoles/Nintendo/NES", "Nintendo NES");
  tree.insert("Consoles/Nintendo/SNES", "Nintento SNES");
  tree.insert("Consoles/Sega/MegaDrive", "MegaDrive");

  auto& iconSet = tree.icon("test");
  iconSet.normal = nes_icon;


  //auto id = tree.icon("icons/nes_icon.png");
  item.icon("test");
  

  form.show();
  nana::exec();
}