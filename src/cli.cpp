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

#include "base/common.h"

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
    DRAW_HEADER = 0x0040
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
    STYLE_COUNT
  };
  
  enum position
  {
    START,
    AFTER_HEADER,
    BETWEEN_ROWS,
    END
  };
  
  using edge_style = std::array<std::string, STYLE_COUNT>;
  
  edge_style asciiStyle()
  {
    return { "+","+","+","+",  "|","-","|","-","|","-",  "+","+",  "+","+","+","+",  "+","+","+","+" };
  }
  
  edge_style utf8Style()
  {
    return { "┏","┓","┗","┛",  "┃","━","│","─","┃","━",  "┼","╇",  "┯","┷","┨","┠",  "┳","┻","┫","┣" };
  }
  
  edge_style unixStyle()
  {
    return { "┌","┐","└","┘",  "│","─","│","─","│","─",  "┼","┼",  "┬","┴","┤","├",  "┬","┴","┤","├" };
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
        width = std::max(width, rows[r][i].size());
      
      columns[i].width = width;
    }
  }
  
  void pad(size_t width) { out << std::string(width, ' '); }
  void ln() { out << std::endl; }
  
public:
  
  ascii_table(std::ostream& out) : out(out), style(unixStyle())
  {
    setFlag(flag::DRAW_LEFT_EDGE, true);
    setFlag(flag::DRAW_RIGHT_EDGE, true);
    setFlag(flag::DRAW_TOP_EDGE, true);
    setFlag(flag::DRAW_BOTTOM_EDGE, true);
    setFlag(flag::DRAW_BETWEEN_ROW_EDGE, true);
    setFlag(flag::DRAW_HEADER, true);

  }

  void addRow(const std::vector<std::string>& row)
  {
    rows.push_back(row);
  }
  
  void addColumn(const std::initializer_list<std::string>& names)
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
        
      case position::BETWEEN_ROWS:
      {
        start = edge::MIDDLE_RIGHT_ROW;
        hor = edge::HORIZONTAL_ROW;
        sep = edge::CROSS;
        end = edge::MIDDLE_LEFT_ROW;
        break;
      }
        
      case position::AFTER_HEADER:
      {
        start = edge::MIDDLE_RIGHT_BOLD;
        hor = edge::HORIZONTAL_AFTER_HEADER;
        sep = edge::CROSS_AFTER_HEADER;
        end = edge::MIDDLE_LEFT_BOLD;
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
    out << ((flags && flag::DRAW_LEFT_EDGE) ? style[VERTICAL_EDGE] : " ");

    for (int c = 0; c < columns.size(); ++c)
    {
      const ColumnSpec& column = columns[c];
      const padding padding = isHeader ? column.titlePadding : column.rowPadding;
      const size_t margin = column.margin;
      const size_t width = column.width;
      
      pad(margin);
      
      const std::string& text = data[c];
      size_t leftOver = width - text.size();
      assert(leftOver >= 0);
      
      if (padding == padding::LEFT)
        out << std::string(leftOver, ' ') << text;
      else if (padding == padding::RIGHT)
        out << text << std::string(leftOver, ' ');
      else
      {
        size_t leftPad = leftOver / 2 + (leftOver % 2 != 0 ? 1 : 0);
        size_t rightPad = leftOver / 2;
        out << std::string(leftPad, ' ') << text << std::string(rightPad, ' ');
      }
      
      pad(margin);
      
      if (flags && flag::DRAW_RIGHT_EDGE)
      {
        out << ((isHeader || c == columns.size()-1) ? style[VERTICAL_EDGE] : style[VERTICAL_ROW]);
      }
      
      
    }
    
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

int main(int argc, const char* argv[])
{
  ascii_table table(std::cout);
  
  table.addColumn({ "SIZE", "CRC32", "MD5" });
  table.addRow({ "32.1Gb", "0xAB12CD34", "312312c1d23c1231d23c3" });
  table.addRow({ "32.1Gb", "0xAB12CD34", "123121112" });

  table.setRowPadding(2, ascii_table::padding::CENTER);
  
  table.printTable();

  return 0;
}
