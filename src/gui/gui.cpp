#include <cstdio>

#include "nana/gui.hpp"
#include "nana/gui/widgets/label.hpp"
#include "nana/gui/widgets/treebox.hpp"
#include "nana/gui/widgets/toolbar.hpp"
#include "nana/gui/widgets/textbox.hpp"
#include "nana/gui/widgets/menubar.hpp"
#include "nana/gui/widgets/listbox.hpp"
#include "nana/gui/widgets/menu.hpp"
#include "nana/gui/place.hpp"

#include "box/archive.h"

#include "tbx/streams/file_data_source.h"
#include "box/archive_builder.h"

struct GUI
{
  nana::listbox* table = nullptr;
  nana::form* form = nullptr;
};

GUI gui;


void loadFile(const path& filename)
{
  auto source = file_data_source(filename);
  Archive archive;
  archive.read(source);

  gui.table->clear();

  nana::size::value_type width = 0;
  nana::paint::graphics g = nana::paint::graphics(gui.table->size());
  g.typeface(gui.form->typeface());

  for (const auto& entry : archive.entries())
  {
    width = std::max(width, g.text_extent_size(entry.name()).width);

    const auto& binary = entry.binary();

    std::string filteredSize = strings::humanReadableSize(binary.filteredSize, false);
    std::string size = strings::humanReadableSize(binary.digest.size, false);

    gui.table->at(0)->append({entry.name(), filteredSize, size });
  }

  gui.table->column_at(0).width(width + 20);
}

int main(int argc, char* argv[])
{
  if (false)
  {
    ArchiveBuilder builder(CachePolicy(CachePolicy::Mode::NEVER, 0), MB128, MB128);
    auto sources = builder.buildSourcesFromFolder(R"(C:\Users\Jack\Desktop\patapon)");
    auto archive = builder.buildSingleStreamBaseWithDeltasArchive(sources, 0);
    memory_buffer sink;
    archive.options().bufferSize = MB64;
    archive.write(sink);
    sink.serialize(file_handle(R"(C:\Users\Jack\Desktop\patapon\patapon.box)", file_mode::WRITING));
  }
  
  
  /* basic nana form */
  nana::form form(nana::API::make_center(800, 600));
  form.caption("Box Archive Manager v0.1");
  gui.form = &form;

  nana::menubar menubar(form);

  menubar.push_back("File");
  menubar.at(0).append("Open");
  menubar.at(0).append_splitter();
  menubar.at(0).append("Exit", [&form](auto) {
    form.close();
  });

  nana::toolbar toolbar(form);

  nana::label status(form, "Ready");
  status.text_align(nana::align::left, nana::align_v::center);
  status.transparent(true);
  status.bgcolor(nana::colors::button_face);

  nana::listbox table(form);

  table.append_header("Name", 200);
  table.append_header("Compressed Size", 150);
  table.append_header("Size Size", 150);
  table.checkable(false);
  table.show_header(true);
  table.auto_draw(true);
  gui.table = &table;

  nana::place layout(form);

  layout.div("vert <menu weight=24> <toolbar weight=48> <table> <status weight=24>");
  layout["menu"] << menubar;
  layout["toolbar"] << toolbar;
  layout["table"] << table;
  layout["status"] << status;
  
  layout.collocate();

  //loadFile(R"(C:\Users\Jack\Documents\dev\retrozip\projects\msvc2017\cellar\vault\f0\f071d45d8f5cb05b48d7d2b804c6cb6a79ad96fb.box)");
  loadFile(R"(C:\Users\Jack\Desktop\patapon\patapon.box)");



  form.show();
  nana::exec();
}