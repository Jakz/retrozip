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

#include <cassert>
#include <vector>
#include <iostream>

class ascii_table
{
private:
  std::ostream& out;
  
  enum class Padding
  {
    LEFT,
    RIGHT,
    CENTER
  };
  
  struct ColumnSpec
  {
    std::string name;
    size_t width;
    size_t margin;
    
    Padding titlePadding;
    Padding rowPadding;
  };
  
  std::vector<ColumnSpec> columns;
  std::vector<std::vector<std::string>> rows;
  
  void recomputeWidths()
  {
    const size_t cols = columns.size();
    
    for (size_t i = 0; i < cols; ++i)
    {
      size_t width = columns[i].name.size();
      
      for (size_t r = 0; r < rows.size(); ++r)
        width = std::max(width, rows[r][i].size());
      
      columns[i].width = width;
    }
  }
  
  void pad(size_t width) { out << std::string(width, ' '); }
  void ln() { out << std::endl; }
  
public:
  
  ascii_table(std::ostream& out) : out(out)
  {

  }

  void addRow(const std::vector<std::string>& row)
  {
    rows.push_back(row);
  }
  
  void addColumn(const std::initializer_list<std::string>& names)
  {
    for (const auto& name : names)
    {
      columns.push_back({ name, 0, 1, Padding::LEFT, Padding::RIGHT });
    }
  }
  
  void setRowPadding(const std::initializer_list<Padding>& paddings)
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
  
  void printSeparator()
  {
    out << '+';
    for (const auto& column : columns)
      out << std::string(column.width + column.margin*2, '-') << '+';
    out << std::endl;
  }
  
  /*final private Function<Integer, String> rightPadder = i -> "%1$" + i + "s";
  final private Function<Integer, String> leftPadder = i -> "%1$-" + i + "s";*/
  

  
  
  void printTableRow(const std::vector<std::string>& data, bool isHeader)
  {
    out << "|";
    
    for (int c = 0; c < columns.size(); ++c)
    {
      const ColumnSpec& column = columns[c];
      const Padding padding = isHeader ? column.titlePadding : column.rowPadding;
      const size_t margin = column.margin;
      const size_t width = column.width;
      
      pad(margin);
      
      const std::string& text = data[c];
      size_t leftOver = width - text.size();
      assert(leftOver >= 0);
      
      if (padding == Padding::LEFT)
        out << std::string(leftOver, ' ') << text;
      else if (padding == Padding::RIGHT)
        out << text << std::string(leftOver, ' ');
      else
      {
        size_t leftPad = leftOver / 2 + (leftOver % 2 != 0 ? 1 : 0);
        size_t rightPad = leftOver / 2;
        out << std::string(leftPad, ' ') << text << std::string(rightPad, ' ');
      }
      
      pad(margin);
      out << '|';
    }
    
    ln();
  }
  
  void printTable()
  {
    recomputeWidths();
    printSeparator();
    
    std::vector<std::string> header(columns.size());
    std::transform(columns.begin(), columns.end(), header.begin(), [] (const ColumnSpec& spec) { return spec.name; });
    printTableRow(header, true);
    printSeparator();
    for (const auto& row : rows)
      printTableRow(row, false);
    
    printSeparator();
  }
};

int main(int argc, const char* argv[])
{
  ascii_table table(std::cout);
  
  table.addColumn({ "SIZE", "CRC32", "MD5" });
  table.addRow({ "32.1Gb", "0xAB12CD34", "312312c1d23c1231d23c3" });
  table.addRow({ "32.1Gb", "0xAB12CD34", "123121112" });

  
  table.printTable();

  return 0;
}
