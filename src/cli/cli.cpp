/*#define CATCH_CONFIG_MAIN

#define CATCH_CONFIG_FAST_COMPILE

#include "libs/catch.h"
#include "libs/fmt/printf.h"
#include "cli/args.h"

using namespace args;

static Parser p;

TEST_CASE("dummy")
{
  p.print();
}

TEST_CASE("no arguments")
{
  Arguments a = p.parse({});
  REQUIRE(!a);
  REQUIRE(a.error.message == error::missing_command);
}

TEST_CASE("unknown command")
{
  const std::string bogus = "foo";
  Arguments a = p.parse({ bogus });
  REQUIRE(!a);
  REQUIRE(a.error.message == fmt::sprintf(error::unknown_command, bogus));
}

TEST_CASE("specific command")
{
  Arguments a = p.parse({ "x" });
  REQUIRE(a);
  REQUIRE(a.command == cli::Command::EXTRACT);
}

TEST_CASE("archive names and files list")
{
  SECTION("archive name and no files")
  {
    Arguments a = p.parse({ "x", "archive.box" });
    REQUIRE(a);
    REQUIRE(a.archiveName == "archive.box");
  }
  
  SECTION("archive name and no files")
  {
    Arguments a = p.parse({ "x", "archive.box" });
    REQUIRE(a);
    REQUIRE(a.archiveName == "archive.box");
  }
}*/

#include "tbx/base/common.h"
#include "tbx/base/file_system.h"

#include <cassert>
#include <vector>
#include <array>
#include <iostream>

class ascii_table
{
public:
  enum class padding
  {
    LEFT,
    RIGHT,
    CENTER
  };
  
  enum class flag
  {
    DRAW_LEFT_EDGE = 0x0001,
    DRAW_RIGHT_EDGE = 0x0002,
    DRAW_TOP_EDGE = 0x0004,
    DRAW_HEADER_EDGE = 0x0008,
    DRAW_BETWEEN_ROW_EDGE = 0x0010,
    DRAW_BOTTOM_EDGE = 0x0020,
    DRAW_HEADER = 0x0040,
    DRAW_BETWEEN_COLS_EDGE = 0x0080,
  };
  
private:
  enum edge
  {
    TOP_LEFT = 0,
    TOP_RIGHT,
    BOTTOM_LEFT,
    BOTTOM_RIGHT,
    VERTICAL_EDGE,
    HORIZONTAL_EDGE,
    VERTICAL_ROW,
    HORIZONTAL_ROW,
    VERTICAL_AFTER_HEADER,
    HORIZONTAL_AFTER_HEADER,
    CROSS,
    CROSS_AFTER_HEADER,
    MIDDLE_DOWN_ROW,
    MIDDLE_UP_ROW,
    MIDDLE_LEFT_ROW,
    MIDDLE_RIGHT_ROW,
    MIDDLE_DOWN_BOLD,
    MIDDLE_UP_BOLD,
    MIDDLE_LEFT_BOLD,
    MIDDLE_RIGHT_BOLD,
    VERTICAL_EMPTY,
    CROSS_EMPTY,
    STYLE_COUNT
  };
  
  enum position
  {
    START,
    AFTER_HEADER,
    BETWEEN_ROWS,
    END,
    FORCED
  };
  
  using edge_style = std::array<std::string, STYLE_COUNT>;
  
  edge_style asciiStyle()
  {
    return { "+","+","+","+",  "|","-","|","-","|","-",  "+","+",  "+","+","+","+",  "+","+","+","+", " ", " " };
  }
  
  edge_style utf8Style()
  {
    return { "┏","┓","┗","┛",  "┃","━","│","─","┃","━",  "┼","╇",  "┯","┷","┨","┠",  "┳","┻","┫","┣", " ", " " };
  }
  
  edge_style unixStyle()
  {
    return { "┌","┐","└","┘",  "│","─","│","─","│","─",  "┼","┼",  "┬","┴","┤","├",  "┬","┴","┤","├", " ", " " };
  }
  
  std::ostream& out;

  struct ColumnSpec
  {
    std::string name;
    size_t width;
    size_t margin;
    
    padding titlePadding;
    padding rowPadding;
  };
  
  bit_mask<flag> flags;
  edge_style style;
  std::vector<ColumnSpec> columns;
  std::vector<std::vector<std::string>> rows;
  
  void recomputeWidths()
  {
    const size_t cols = columns.size();
    
    for (size_t i = 0; i < cols; ++i)
    {
      size_t width = columns[i].name.size();
      
      for (size_t r = 0; r < rows.size(); ++r)
      {
        if (i < rows[r].size())
          width = std::max(width, rows[r][i].size());
      }
      
      columns[i].width = width;
    }
  }
  
  void pad(size_t width) { out << std::string(width, ' '); }
  void ln() { out << std::endl; }
  
public:
  
  ascii_table(std::ostream& out) : out(out), style(asciiStyle())
  {
    setFlag(flag::DRAW_LEFT_EDGE, true);
    setFlag(flag::DRAW_RIGHT_EDGE, true);
    setFlag(flag::DRAW_TOP_EDGE, true);
    setFlag(flag::DRAW_BOTTOM_EDGE, true);
    setFlag(flag::DRAW_BETWEEN_ROW_EDGE, true);
    setFlag(flag::DRAW_HEADER, true);
    setFlag(flag::DRAW_BETWEEN_COLS_EDGE, true);

  }

  void addRow(const std::vector<std::string>& row)
  {
    rows.push_back(row);
  }
  
  void addColumn(const std::initializer_list<std::tuple<std::string, padding, padding>>& columns)
  {
    for (const auto& c : columns)
      this->columns.push_back({ std::get<0>(c), 0, 1, std::get<1>(c), std::get<2>(c) });
  }
  
  void addColumnSimple(const std::initializer_list<std::string>& names)
  {
    for (const auto& name : names)
    {
      columns.push_back({ name, 0, 1, padding::LEFT, padding::RIGHT });
    }
  }
  
  void setFlag(flag flag, bool value) { flags.set(flag, value); }
  void setHeaderPadding(size_t index, padding padding) { columns[index].titlePadding = padding; }
  void setRowPadding(size_t index, padding padding) { columns[index].rowPadding = padding; }
  
  void setRowPadding(const std::initializer_list<padding>& paddings)
  {
    assert(paddings.size() <= columns.size());
    
    size_t i = 0;
    for (const auto padding : paddings)
      columns[i++].rowPadding = padding;
  }
  
  void clear()
  {
    columns.clear();
    rows.clear();
  }
  
  void printSeparator(position pos, bool bold = false)
  {
    /*
    if (pos == position::START)
    {
      if (!(flags && flag::DRAW_TOP_EDGE))
        return;
      else if (flags && flag::DRAW_LEFT_EDGE)
        start = style[edge::TOP_LEFT];
    }
    */
    
    edge start = edge::STYLE_COUNT, hor = edge::STYLE_COUNT, sep = edge::STYLE_COUNT, end = edge::STYLE_COUNT;
    
    switch (pos)
    {
      case position::START:
      {
        start = edge::TOP_LEFT;
        hor = edge::HORIZONTAL_EDGE;
        sep = flags && flag::DRAW_HEADER ? edge::MIDDLE_DOWN_BOLD : edge::MIDDLE_DOWN_ROW;
        end = edge::TOP_RIGHT;
        break;
      }
        
      case position::FORCED:
      case position::BETWEEN_ROWS:
      {
        start = flags.isSet(flag::DRAW_LEFT_EDGE) ? edge::MIDDLE_RIGHT_ROW : edge::HORIZONTAL_ROW;
        hor = edge::HORIZONTAL_ROW;
        sep = edge::CROSS;
        end = flags.isSet(flag::DRAW_RIGHT_EDGE) ? edge::MIDDLE_LEFT_ROW : edge::HORIZONTAL_ROW;
        break;
      }
        
      case position::AFTER_HEADER:
      {
        start = flags.isSet(flag::DRAW_LEFT_EDGE) ? edge::MIDDLE_RIGHT_BOLD : edge::HORIZONTAL_AFTER_HEADER;
        hor = edge::HORIZONTAL_AFTER_HEADER;
        sep = edge::CROSS_AFTER_HEADER;
        end = flags.isSet(flag::DRAW_RIGHT_EDGE) ? edge::MIDDLE_LEFT_BOLD : edge::HORIZONTAL_AFTER_HEADER;
        break;
      }
        
      case position::END:
      {
        start = edge::BOTTOM_LEFT;
        hor = edge::HORIZONTAL_EDGE;
        sep = edge::MIDDLE_UP_ROW;
        end = edge::BOTTOM_RIGHT;
        break;
      }
    }
    
    if (!flags.isSet(flag::DRAW_BETWEEN_COLS_EDGE))
      sep = edge::CROSS_EMPTY;
    
    assert(start != edge::STYLE_COUNT && hor != edge::STYLE_COUNT && sep != edge::STYLE_COUNT && end != edge::STYLE_COUNT);
    
    out << style[start];
    
    for (auto col = columns.begin(); col != columns.end(); ++col)
    {
      for (size_t i = 0; i < col->width + col->margin*2; ++i)
        out << style[hor];
      
      out << ((col != columns.end() - 1) ? style[sep] : style[end]);
    }

    out << std::endl;
  }

  void printTableRow(const std::vector<std::string>& data, bool isHeader)
  {
    if (data.empty())
    {
      printSeparator(position::FORCED);
      return;
    }
    
    out << ((flags && flag::DRAW_LEFT_EDGE) ? style[VERTICAL_EDGE] : " ");
    
    for (int c = 0; c < columns.size(); ++c)
    {
      const ColumnSpec& column = columns[c];
      const padding padding = isHeader ? column.titlePadding : column.rowPadding;
      const size_t margin = column.margin;
      const size_t width = column.width;
      
      pad(margin);
      
      const std::string& text = c < data.size() ? data[c] : "";
      size_t leftOver = width - text.size();
      assert(leftOver >= 0);
      
      if (padding == padding::RIGHT)
        out << std::string(leftOver, ' ') << text;
      else if (padding == padding::LEFT)
        out << text << std::string(leftOver, ' ');
      else
      {
        size_t leftPad = leftOver / 2 + (leftOver % 2 != 0 ? 1 : 0);
        size_t rightPad = leftOver / 2;
        out << std::string(leftPad, ' ') << text << std::string(rightPad, ' ');
      }
      
      pad(margin);
      
      if (c < columns.size() - 1)
      {
        if (flags.isSet(flag::DRAW_BETWEEN_COLS_EDGE))
          out << (isHeader ? style[VERTICAL_EDGE] : style[VERTICAL_ROW]);
        else
          out << style[VERTICAL_EMPTY];
      }
    }
    
    if (flags.isSet(flag::DRAW_RIGHT_EDGE))
      out << (isHeader ? style[VERTICAL_EDGE] : style[VERTICAL_ROW]);

    ln();
  }
  
  void printTable()
  {
    recomputeWidths();
    
    if (flags && flag::DRAW_TOP_EDGE)
      printSeparator(position::START);
    
    std::vector<std::string> header(columns.size());
    std::transform(columns.begin(), columns.end(), header.begin(), [] (const ColumnSpec& spec) { return spec.name; });
    printTableRow(header, true);
    printSeparator(position::AFTER_HEADER, true);
    
    for (auto it = rows.begin(); it != rows.end(); ++it)
    {
      printTableRow(*it, false);
      if ((flags && flag::DRAW_BETWEEN_ROW_EDGE) && it != rows.end()-1)
        printSeparator(position::BETWEEN_ROWS);
    }
    
    if (flags && flag::DRAW_BOTTOM_EDGE)
      printSeparator(position::END);
  }
};

#include "tbx/streams/file_data_source.h"
#include "box/archive.h"

class cli
{
public:
  enum class SizeMode
  {
    BYTES,
    READABLE
  };
  
  struct CommonOptions
  {
    bool verbose = true;
    bool debug = false;
  };
  
  struct ListArchiveOptions : public CommonOptions
  {
    bool showCRC32 = false;
    bool showMD5andSHA1 = false;
    bool showFilteredSize = true;
    bool showFilterChain = true;
    SizeMode sizeMode = SizeMode::READABLE;
  };
  
  struct CreateArchiveOptions : public CommonOptions
  {
    path_filter filter = [] (const path& p) { return true; };
    bool recursiveScan = true;
  };
  
  static std::string size_to_string(SizeMode mode, u64 size)
  {
    return mode == SizeMode::BYTES ?
    fmt::sprintf("%zu bytes", size) :
    strings::humanReadableSize(size, true, 2);
  }
  
  
  static void addSizeValueToRow(const ListArchiveOptions& options, std::vector<std::string>& row, size_t size)
  {
    row.push_back(options.sizeMode == SizeMode::BYTES ?
                  std::to_string(size) :
                  strings::humanReadableSize(size, true)
    );
  }

public:
  static void printArchiveInformation(const class path& path, const Archive& archive);
  static void listArchiveContent(const ListArchiveOptions& options, const Archive& archive);
  static void createArchive(const std::vector<path>& sources, const class path& dest, const CreateArchiveOptions& options);
};


void cli::printArchiveInformation(const class path& path, const Archive& archive)
{
  using namespace std;
  
  ArchiveSizeInfo info = archive.sizeInfo();
  
  SizeMode mode = SizeMode::READABLE;
  
  cout << endl;
  cout << "Path : " << path << endl;
  cout << "Size on disk : " << size_to_string(mode, info.totalSize) << endl;
  cout << "Uncompressed data size : " << size_to_string(mode, info.uncompressedEntriesData) << endl;
  cout << "Compression ratio: " << fmt::sprintf("%1.2f (%d%%)", (float)info.uncompressedEntriesData / info.totalSize, (int) ((info.totalSize / (float)info.uncompressedEntriesData) * 100)) << endl;
  cout << "Bytes used for data : " << size_to_string(SizeMode::BYTES, info.streamsData) << endl;
  cout << "Bytes used for structure : " << size_to_string(SizeMode::BYTES, (info.totalSize - info.streamsData)) << endl;
  cout << "Content : " << archive.entries().size() << " entries in " << archive.streams().size() << " streams" << endl;

  cout << endl;
}

void cli::createArchive(const std::vector<path>& sources, const path& dest, const cli::CreateArchiveOptions& options)
{
  using namespace std;

  vector<path> files;
  
  for (const path& source : sources)
  {
    if (source.isFolder())
    {
      auto sfiles = FileSystem::i()->contentsOfFolder(source, options.recursiveScan, [&options] (const path& p) { return !options.filter(p); });
      files.insert(begin(sfiles), end(sfiles), end(files));
    }
  }
  
  if (options.verbose)
    cout << files.size() << " entries are going to be archived." << endl;
}

void cli::listArchiveContent(const ListArchiveOptions& options, const Archive& archive)
{
  using p = ascii_table::padding;
  using f = ascii_table::flag;
  
  ascii_table table(std::cout);
  table.setFlag(f::DRAW_BETWEEN_ROW_EDGE, false);
  table.setFlag(f::DRAW_TOP_EDGE, false);
  table.setFlag(f::DRAW_BOTTOM_EDGE, false);
  table.setFlag(f::DRAW_LEFT_EDGE, false);
  table.setFlag(f::DRAW_RIGHT_EDGE, false);
  table.setFlag(f::DRAW_BETWEEN_COLS_EDGE, false);

  
  table.addColumn({ std::make_tuple("SIZE", p::RIGHT, p::RIGHT) });

  if (options.showFilteredSize)
    table.addColumn({ std::make_tuple("FILTERED", p::RIGHT, p::RIGHT) });

  if (options.showCRC32)
    table.addColumn({ std::make_tuple("CRC", p::CENTER, p::CENTER) });
  if (options.showMD5andSHA1)
    table.addColumnSimple({ "MD5", "SHA1" });
  
  if (options.showFilterChain)
    table.addColumnSimple({"FILTERS"});
  
  table.addColumn({ std::make_tuple("S:I", p::RIGHT, p::RIGHT), std::make_tuple("NAME", p::LEFT, p::LEFT) });
  
  for (const auto& entry : archive.entries())
  {
    const auto& binary = entry.binary();
    
    std::vector<std::string> row;
    
    addSizeValueToRow(options, row, binary.digest.size);
    
    if (options.showFilteredSize)
    {
      if (binary.filteredSize != binary.digest.size)
        addSizeValueToRow(options, row, binary.filteredSize);
      else
        row.push_back("");
    }

    //TODO: choose if all uppercase or not
    if (options.showCRC32)
    {
      row.push_back(fmt::sprintf("%08X", binary.digest.crc32));
    }
    
    if (options.showMD5andSHA1)
    {
      row.push_back(binary.digest.md5);
      row.push_back(binary.digest.sha1);
    }
    
    if (options.showFilterChain)
    {
      std::string entryMnemonic = entry.filters().mnemonic(true);
      std::string streamMnemonic = archive.streams()[entry.binary().stream].filters().mnemonic(true);
      
      if (!entryMnemonic.empty() && !streamMnemonic.empty())
        row.push_back(entryMnemonic + ";" + streamMnemonic);
      else
        row.push_back(entryMnemonic + streamMnemonic);
    }
    
    row.push_back(fmt::sprintf("%lu:%lu", binary.stream, binary.indexInStream));
    row.push_back(entry.name());
    
    table.addRow(row);
  }
  
  table.addRow({});
  
  ArchiveSizeInfo sizeInfo = archive.sizeInfo();
  size_t totalSizeUncompressed = sizeInfo.uncompressedEntriesData;
  
  std::vector<std::string> summary;
  
  addSizeValueToRow(options, summary, totalSizeUncompressed);
  
  if (options.showFilteredSize)
    summary.push_back("");
  
  if (options.showCRC32) //TODO: could be file checksum?
    summary.push_back("");
  
  if (options.showMD5andSHA1)
  {
    summary.push_back("");
    summary.push_back("");
  }
  
  summary.push_back("");
  summary.push_back("");
  summary.push_back(fmt::sprintf("%lu files in %lu streams", archive.entries().size(), archive.streams().size()));
  
  table.addRow(summary);
  
  table.printTable();
}

#include "cli/box.h"
int disabled3(int argc, const char* argv[])
{
  auto* handle = boxOpenArchive("output.box");
  
  EntryInfo info;
  boxFillEntryInfo(handle, 0, &info);

  printf("%s\n", info.name);
  return 0;
}

#define CATCH_CONFIG_MAIN
//#include "test/catch.h"

#include "cli/cxxopts.hpp"

int mainzzz(int argc, const char* argv[])
{
  cxxopts::Options options("box", "box archive cli interface");

  options.add_options()
    ("i,input", "input file name", cxxopts::value<std::vector<std::string>>())
    ("output", "output file name", cxxopts::value<std::string>())
  ;

  options.parse_positional("output");

  printf("%s\n", options.help().c_str());

  options.parse(argc, argv);
  return 0;
}

#include "box/archive_builder.h"
int main(int argc, const char* argv[])
{
  //auto session = Catch::Session();
  //return session.run(argc, argv);

  path output = R"(F:\Misc\retrozip\smas\output.box)";
 
  Archive archive;
  
  bool build = true;
  if (build)
  {
    path path = R"(F:\Misc\retrozip\smash\supermono)";

    ArchiveBuilder builder(CachePolicy(CachePolicy::Mode::NEVER, 0), MB128, MB128);

    auto sources = builder.buildSourcesFromFolder(path);
    std::cout << "Found " << sources.size() << " files to archive." << std::endl;

    archive = builder.buildSingleStreamBaseWithDeltasArchive(sources, 0);// builder.buildSingleStreamSolidArchive(sources);
    memory_buffer sink;
    archive.options().bufferSize = MB1;
    archive.write(sink);
    sink.serialize(file_handle(output, file_mode::WRITING));
  }
  else if (false)
  {    
    file_data_source source("output.box");
    archive.options().bufferSize = MB256;
    archive.read(source);

    memory_buffer sink;
    archive.write(sink);
    sink.serialize(file_handle("output2.box", file_mode::WRITING));
  }

  cli::printArchiveInformation(output, archive);

  cli::ListArchiveOptions options;

  cli::listArchiveContent(options, archive);

  //ArchiveBuilder builder(CachePolicy(CachePolicy::Mode::ALWAYS, 0), MB256, MB256);
  //builder.extractSpecificFilesFromArchive(R"(F:\\Misc\\retrozip\\output.box)", "", 0);
  
  return 0;
}

#include "box/archive_builder.h"
int disabled(int argc, const char* argv[])
{
  if (argc != 2)
  {
    std::cout << "Usage: retrozip <path-containing-roms>" << std::endl;
    return -1;
  }
  
  path path = argv[1];
  
  if (!path.exists())
  {
    std::cerr << "Path specified doesn't exist!" << std::endl;
    return -1;
  }
  
  ArchiveBuilder builder(CachePolicy(CachePolicy::Mode::ALWAYS, 0), MB16, MB16);
  
  auto sources = builder.buildSourcesFromFolder(path);
  std::cout << "Found " << sources.size() << " files to archive." << std::endl;
  
  Archive archive = builder.buildSingleStreamBaseWithDeltasArchive(sources, 0);
  memory_buffer sink;
  archive.options().bufferSize = MB32;
  archive.write(sink);
  sink.serialize(file_handle("output.box", file_mode::WRITING));
  
  cli::printArchiveInformation("output.box", archive);
  
  cli::ListArchiveOptions options;
  
  options.showMD5andSHA1 = true;
  cli::listArchiveContent(options, archive);
  
  return 0;
}
