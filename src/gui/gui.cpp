#include <cstdio>

#include "nana/gui.hpp"
#include "nana/gui/widgets/label.hpp"
#include "nana/gui/widgets/treebox.hpp"
#include "nana/gui/widgets/toolbar.hpp"
#include "nana/gui/widgets/textbox.hpp"
#include "nana/gui/widgets/menubar.hpp"
#include "nana/gui/widgets/listbox.hpp"
#include "nana/gui/widgets/progress.hpp"
#include "nana/gui/widgets/menu.hpp"
#include "nana/gui/place.hpp"

#include "box/archive.h"

#include "tbx/streams/file_data_source.h"
#include "box/archive_builder.h"

struct GUI
{
  path filepath;
  Archive archive;

  nana::form form;
  nana::listbox table{ form };
  
  nana::label* status = nullptr;

  struct
  {
    nana::form* form;
    nana::progress* bar;
    nana::label* label;
  } progress;

  void setStatusText(const std::string& text)
  {
    status->caption(text);
  }

  void setExtractionProgress(float progress)
  {
    this->progress.bar->value(int(progress * 100.0f));
  }

  void loadFile(const path& filename);
  void extractNth(int index, const path& folder)
  {
    size_t total = archive.entries()[index].binary().digest.size;
    
    ArchiveBuilder builder(CachePolicy(CachePolicy::Mode::NEVER, 0), MB128, MB128);
    builder.extractSpecificFilesFromArchive(filepath, folder, index, [this, total](float bytes) {
      printf("extracted %f\n", bytes / float(total));
      this->progress.bar->value( int(bytes / float(total) * 100.0f));
    });
  }

  void init();
};

GUI* gui = nullptr;

void GUI::init()
{
  form.size(nana::size(800, 600));
  form.move(nana::API::make_center(form.size()));
  form.caption("Box Archive Manager v0.1");

  table.append_header("Name", 200);
  table.append_header("Compressed Size", 150);
  table.append_header("Size Size", 150);
  table.checkable(false);
  table.show_header(true);
  table.auto_draw(true);

  nana::menu contextMenu;
  contextMenu.append("Extract here", [this](nana::menu::item_proxy item) {
    auto selection = table.selected();

    std::thread([selection, this]() {
      for (const auto& entry : selection)
        extractNth(entry.item, filepath.parent());

      nana::API::refresh_window(progress.form->handle());
      nana::API::close_window(progress.form->handle());
      }).detach();

    progress.form->modality();
  });
  contextMenu.append("Extract to...", [](nana::menu::item_proxy) { nana::msgbox("Open") << "Opening game..."; });


  int row_height = table.scheme().item_height_ex; // typically 24

  table.events().mouse_up([&](const nana::arg_mouse& arg)
    {
      auto index = table.cast(arg.pos).item;

      if (arg.button == nana::mouse::right_button && index != -1)
      {
        contextMenu.popup(form, arg.pos.x, arg.pos.y);
      }
  });
}


void GUI::loadFile(const path& filename)
{
  filepath = filename;
  
  auto source = file_data_source(filename);
  archive.read(source);

  table.clear();

  nana::size::value_type width = 0;
  nana::paint::graphics g = nana::paint::graphics(table.size());
  g.typeface(form.typeface());

  for (const auto& entry : archive.entries())
  {
    width = std::max(width, g.text_extent_size(entry.name()).width);

    const auto& binary = entry.binary();

    std::string filteredSize = strings::humanReadableSize(binary.filteredSize, false);
    std::string size = strings::humanReadableSize(binary.digest.size, false);

    table.at(0)->append({entry.name(), filteredSize, size });
  }

  table.column_at(0).width(width + 20);

  setStatusText(filename.str());
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

  GUI gui;
  
  
  /* basic nana form */
  gui.init();


  nana::menubar menubar(gui.form);

  menubar.push_back("File");
  menubar.at(0).append("Open");
  menubar.at(0).append_splitter();
  menubar.at(0).append("Exit", [&gui](auto) {
    gui.form.close();
    nana::API::exit_all();
  });

  nana::toolbar toolbar(gui.form);

  nana::label status(gui.form, "Ready");
  status.text_align(nana::align::left, nana::align_v::center);
  status.transparent(true);
  status.bgcolor(nana::colors::button_face);
  gui.status = &status;

  nana::place layout(gui.form);

  layout.div("vert <menu weight=24> <toolbar weight=48> <table> <status weight=24>");
  layout["menu"] << menubar;
  layout["toolbar"] << toolbar;
  layout["table"] << gui.table;
  layout["status"] << status;
  
  layout.collocate();

  gui.progress.form = new nana::form();
  gui.progress.bar = new nana::progress(*gui.progress.form, nana::rectangle(0, 0, 200, 20));

  {
    gui.progress.form->caption("Extracting...");
    gui.progress.form->size({ 400, 100 });

    gui.progress.form->div("vert <progress>");
    gui.progress.form->operator[]("progress") << *gui.progress.bar;
  }


  //loadFile(R"(C:\Users\Jack\Documents\dev\retrozip\projects\msvc2017\cellar\vault\f0\f071d45d8f5cb05b48d7d2b804c6cb6a79ad96fb.box)");
  gui.loadFile(R"(C:\Users\Jack\Desktop\patapon\patapon.box)");



  gui.form.show();
  nana::exec();
}