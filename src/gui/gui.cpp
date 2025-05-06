#include <cstdio>

#include "nana/gui.hpp"
#include "nana/system/dataexch.hpp"
#include "nana/gui/widgets/label.hpp"
#include "nana/gui/widgets/treebox.hpp"
#include "nana/gui/widgets/toolbar.hpp"
#include "nana/gui/widgets/textbox.hpp"
#include "nana/gui/widgets/menubar.hpp"
#include "nana/gui/widgets/listbox.hpp"
#include "nana/gui/widgets/progress.hpp"
#include "nana/gui/widgets/menu.hpp"
#include "nana/gui/filebox.hpp"
#include "nana/gui/place.hpp"

#include "box/archive.h"

#include "tbx/streams/file_data_source.h"
#include "box/archive_builder.h"

#include "tbx/extra/subprocess.hpp"

struct GUI
{
  path filepath;
  Archive archive;

  nana::paint::font monospaced{"Consolas", 10.0f};

  nana::form form;
  nana::listbox table{ form };

  nana::menu contextMenu;
  
  nana::label* status = nullptr;

  struct
  {
    nana::form form{nana::API::make_center(400, 100), nana::appearance(false, false, true, false, false, false, false) };
    nana::progress bar{ form };
    nana::label label{ form };
  } progress;

  nana::filebox openBox{ form, true };

  void setStatusText(const std::string& text)
  {
    status->caption(text);
  }

  void setExtractionProgress(float progress)
  {
    this->progress.bar.value(int(progress * 100.0f));
  }

  void loadFile(const path& filename);
  void extractNth(int index, const path& folder)
  {
    size_t total = archive.entries()[index].binary().digest.size;
    
    ArchiveBuilder builder(CachePolicy(CachePolicy::Mode::NEVER, 0), MB128, MB128);
    builder.extractSpecificFilesFromArchive(filepath, folder, index, [this, total](float bytes) {
      printf("extracted %f\n", bytes / float(total));
      this->progress.bar.value( int(bytes / float(total) * 100.0f));
    });
  }

  void init();
};

void GUI::init()
{  
  form.size(nana::size(1280, 600));
  form.move(nana::API::make_center(form.size()));
  form.caption("Box Archive Manager v0.1");
  form.events().unload([](const auto&) {
    nana::API::exit_all();
  });

  table.append_header("Name", 200);
  table.append_header("Compressed Size", 120);
  table.append_header("Size", 120);
  table.append_header("Stream:Entry", 100);
  table.append_header("CRC32", 100);
  table.append_header("SHA-1", 250);
  table.append_header("Mode", 200);

  table.checkable(false);
  table.show_header(true);
  table.auto_draw(true);

  for (size_t i = 1; i <= 5; ++i)
  {
    table.column_at(i).text_align(nana::align::right);
  }

  contextMenu.append("Extract here", [this](nana::menu::item_proxy item) {
    auto selection = table.selected();

    setExtractionProgress(0.0f);
    nana::API::show_window(progress.form.handle(), true);

    std::thread([selection, this]() {
      for (const auto& entry : selection)
        extractNth(entry.item, filepath.parent());

      nana::API::refresh_window(progress.form.handle());
      nana::API::show_window(progress.form.handle(), false);
      }).detach();

    progress.form.modality();
  });
  contextMenu.append("Extract to...", [](nana::menu::item_proxy) { nana::msgbox("Open") << "Opening game..."; });
  contextMenu.append_splitter();
  contextMenu.append("Copy Filename", [this](nana::menu::item_proxy item) {
    auto selection = table.selected();
    auto index = selection.begin()->item;
    nana::system::dataexch().set(archive.entries()[index].name());
  });
  contextMenu.append("Copy CRC32", [this](nana::menu::item_proxy item) {
    auto selection = table.selected();
    auto index = selection.begin()->item;
    nana::system::dataexch().set(fmt::format("{:08X}", archive.entries()[index].binary().digest.crc32));
  });
  contextMenu.append("Copy SHA-1", [this](nana::menu::item_proxy item) {
    auto selection = table.selected();
    auto index = selection.begin()->item;
    nana::system::dataexch().set(archive.entries()[index].binary().digest.sha1.literal());
  });
  contextMenu.append("Copy MD5", [this](nana::menu::item_proxy item) {
    auto selection = table.selected();
    auto index = selection.begin()->item;
    nana::system::dataexch().set(archive.entries()[index].binary().digest.md5.literal());
  });


  int row_height = table.scheme().item_height_ex; // typically 24

  table.events().mouse_up([&](const nana::arg_mouse& arg)
    {
      auto index = table.cast(arg.pos).item;

      if (arg.button == nana::mouse::right_button && index != -1)
      {
        contextMenu.popup(form, arg.pos.x, arg.pos.y);
      }
  });

  {
    progress.form.caption("Extracting");
    
    progress.bar.size(nana::size(200, 20));

    {
      progress.form.caption("Extracting...");
      progress.form.size({ 400, 100 });

      progress.form.div("vert <progress>");
      progress.form["progress"] << progress.bar;
    }
  }

  {
    openBox.add_filter("Box Archive", "*.box");
    openBox.add_filter("All Files", "*.*");
  }
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

  size_t i = 0;

  for (const auto& entry : archive.entries())
  {
    width = std::max(width, g.text_extent_size(entry.name()).width);

    const auto& binary = entry.binary();

    std::string filteredSize = strings::humanReadableSize(binary.filteredSize, false);
    std::string size = strings::humanReadableSize(binary.digest.size, false);
    std::string entryInfo = fmt::format("{}:{}", binary.stream, i);
    std::string crc32 = fmt::format("{:08X}", binary.digest.crc32);
    std::string sha1 = binary.digest.sha1.literal();

    std::string streamMode = archive.streams()[entry.binary().stream].filters().mnemonic(true);
    std::string entryMode = entry.filters().mnemonic(true);
    std::string mode;

    if (streamMode.empty() && entryMode.empty())
      mode = "null(null)";
    else if (streamMode.empty())
      mode = fmt::format("null({})", entryMode);
    else if (entryMode.empty())
      mode = fmt::format("{}(null)", streamMode);
    else
      mode = fmt::format("{}({})", streamMode, entryMode);

    table.at(0)->append({ entry.name(), filteredSize, size, entryInfo, crc32, sha1, mode });

    for (size_t j = 1; j < table.at(0)->columns(); ++j)
    {
      //table.at(0)->at(i).bgcolor;
    }

    ++i;

    size_t iii = entry.binary().stream;
    table.at(0)->at(table.at(0).size()-1).bgcolor(iii % 2 == 0 ? nana::colors::white_smoke : nana::colors::white);
  }

  table.column_at(0).width(width + 20);

  setStatusText(filename.str());
}

#include <thread>

#include "flow/flow.h"

int main(int argc, char* argv[])
{
  if (false)
  {
    ArchiveBuilder builder(CachePolicy(CachePolicy::Mode::NEVER, 0), MB128, MB128);
    std::vector<FileGroup> groups;

    groups.push_back(FileGroup::ofSolid(path(R"(C:\Users\Jack\Desktop\patapon\smb3)").contents()));
    groups.push_back(FileGroup::ofBaseWithDelta(path(R"(C:\Users\Jack\Desktop\patapon\files)").contents(), 0));
    
    auto archive = builder.build(groups);
    memory_buffer sink;
    archive.options().bufferSize = MB64;
    archive.write(sink);
    sink.serialize(file_handle(R"(C:\Users\Jack\Desktop\patapon\whole.box)", file_mode::WRITING));
  }

  if (false)
  {
    ArchiveBuilder builder(CachePolicy(CachePolicy::Mode::NEVER, 0), MB128, MB128);

    auto sources = builder.buildSourcesFromFolder(R"(C:\Users\Jack\Desktop\patapon\smb3)");
    auto archive = builder.buildSingleStreamSolidArchive(sources);
    memory_buffer sink;
    archive.options().bufferSize = MB1;
    archive.write(sink);
    sink.serialize(file_handle(R"(C:\Users\Jack\Desktop\patapon\smb3.box)", file_mode::WRITING));
  }

  GUI gui;
  
  
  /* basic nana form */
  gui.init();


  nana::menubar menubar(gui.form);

  menubar.push_back("File");
  menubar.at(0).append("Open", [&gui](auto) {
    gui.openBox.init_path(gui.filepath.parent().fspath());
    auto paths = gui.openBox();
    
    if (!paths.empty())
    {
      path path = ::path(paths[0].native());
      gui.loadFile(path);
    }
  });

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


  //loadFile(R"(C:\Users\Jack\Documents\dev\retrozip\projects\msvc2017\cellar\vault\f0\f071d45d8f5cb05b48d7d2b804c6cb6a79ad96fb.box)");
  gui.loadFile(R"(C:\Users\Jack\Desktop\patapon\whole.box)");



  gui.form.show();

  flow::InputFile input(R"(C:/Users/Jack/Desktop/patapon/files/1379 - Patapon (USA).iso)");
  flow::InputFile input2(R"(C:/Users/Jack/Desktop/patapon/files/1364 - Patapon (Europe) (En,Fr,De,Es,It).iso)");

  flow::OutputFile output(R"(C:/Users/Jack/Desktop/patapon/output.cso)");
  flow::OutputFile outputZip(R"(C:/Users/Jack/Desktop/patapon/output.zip)");
  flow::Parameters params(&input, &outputZip);
  params.addInput(&input2);
  
  flow::commands::InputToZip command;

  flow::ProgressLambdaReporter reporter([&gui](float percent) {
    int width = 20;
    int pos = static_cast<int>(width * percent);
    std::cout << "\r[";
    for (int i = 0; i < width; ++i)
      std::cout << (i < pos ? "=" : " ");
    std::cout << "] " << std::fixed << std::setprecision(1) << (percent * 100) << "%";
    std::cout.flush();
  });

  auto result = command.runAsync(params, &reporter);
  result.wait_for(std::chrono::seconds(30));

  if (false)
  {
    namespace sp = subprocess;

    //auto process = sp::Popen({ "tools/maxcso/maxcso.exe", "C:/Users/Jack/Desktop/patapon/files/1379 - Patapon (USA).iso", "-o", "C:/Users/Jack/Desktop/patapon/output.cso"}, sp::output{sp::PIPE}, sp::error{sp::PIPE});
    //auto result = process.communicate();

    std::future<std::pair<sp::OutBuffer, sp::ErrBuffer>> future = std::async(std::launch::async, [] {
      subprocess::Popen proc(
        { "tools/maxcso/maxcso.exe", "C:/Users/Jack/Desktop/patapon/files/1379 - Patapon (USA).iso", "-o", "C:/Users/Jack/Desktop/patapon/output.cso" },
        subprocess::output{ sp::PIPE },
        subprocess::error{ sp::PIPE });
      return proc.communicate();
      });

    while (future.wait_for(std::chrono::milliseconds(100)) != std::future_status::ready) {
      std::cout << "Still working...\n";
    }

    // When done:
    auto [stdout_str, stderr_str] = future.get();
  }

  //std::cout << "Data : " << std::string(&result.second.buf[0]) << std::endl;

  nana::exec();
}