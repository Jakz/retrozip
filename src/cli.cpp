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

#include <vector>
#include <iostream>

class ASCIITablePrinter
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
    int width;
    int margin;
    
    Padding titlePadding;
    Padding rowPadding;
  };
  
  std::vector<ColumnSpec> columns;
  std::vector<std::vector<std::string>> rows;
  
  void recomputeWidths()
  {
    final int cols = columns.size();
    
    for (int i = 0; i < cols; ++i)
    {
      int j = i;
      
      Stream<List<String>> measurer = Stream.concat(Stream.of(columns.stream().map(c -> c.name).collect(Collectors.toList())), rows.stream());
      columns.get(j).width = measurer.map(row -> row.get(j)).mapToInt(String::length).max().getAsInt();
    }
  }
  
public:
  
  ASCIITablePrinter(std::ostream& out) : out(out)
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
      columns.push_back({ name, 0, 1, Padding.LEFT, Padding.RIGHT });
    }
  }
  
  void setRowPadding(const std::initializer_list<Padding>& paddings)
  {
    assert(paddings.size() <= columns.size());
    
    for (size_t i = 0; i < paddings.length; ++i)
      columns[i].rowPadding = paddings[i];
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
  
  final private Function<Integer, String> rightPadder = i -> "%1$" + i + "s";
  final private Function<Integer, String> leftPadder = i -> "%1$-" + i + "s";
  
  private void pad(int width) { for (int m = 0; m < width; ++m) wrt.write(" "); }
  private void ln() { wrt.write("\n"); }
  
  
  public void printTableRow(List<String> data, boolean isHeader) throws IOException
  {
    
    
    wrt.write("|");
    for (int c = 0; c < columns.size(); ++c)
    {
      final ColumnSpec column = columns.get(c);
      final Padding padding = isHeader ? column.titlePadding : column.rowPadding;
      final int margin = column.margin;
      final int width = column.width;
      
      pad(margin);
      
      if (padding != Padding.CENTER)
        wrt.write(String.format(padding == Padding.LEFT ? leftPadder.apply(width) : rightPadder.apply(width), data.get(c)));
        else
        {
          int leftOver = width - data.get(c).length();
          int leftPad = leftOver / 2 + (leftOver % 2 != 0 ? 1 : 0);
          int rightPad = leftOver / 2;
          pad(leftPad);
          wrt.write(data.get(c));
          pad(rightPad);
        }
      
      pad(margin);
      
      wrt.write("|");
    }
    ln();
  }
  
  public void printTable() throws IOException
  {
    recomputeWidths();
    printSeparator();
    printTableRow(columns.stream().map(c -> c.name).collect(Collectors.toList()), true);
    printSeparator();
    for (List<String> row : rows)
      printTableRow(row, false);
      printSeparator();
  }
}

class ascii_table
{
  
};


void main(int argc, const char* argv[])
{
  
  
  
  return 0;
}
