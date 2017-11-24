#include "args.h"

#include <iostream>

using namespace args;

Parser::Parser()
{
  program = "rzip";
  version = "0.1a";
  copyright = "";
  
  commands = {
    { "l", "list contents of archive" },
    { "t", "test integrity of archive" },
    { "x", "extract file with full paths" },
    { "s", "create single stream archive" }
  };
  
  std::sort(commands.begin(), commands.end(), [](const Command& c1, const Command& c2) { return c1.command < c2.command; });
}

void Parser::print()
{
  std::cout << std::endl;
  std::cout << program << " " << version << copyright << std::endl << std::endl;
  std::cout << "Usage:  " << program << " <command> [<switches>...] <archive_name> [<file_names>...]" << std::endl << std::endl;
  
  std::cout << "<Commands>" << std::endl;
  for (const auto& command : commands)
    std::cout << "  " << command.command << " : " << command.help << std::endl;
}

Arguments Parser::parse(const std::vector<std::string>& args)
{
  if (args.empty())
    return Arguments(error::missing_command);
  
  const auto command = std::find(commands.begin(), commands.end(), args[0]);
  
  if (command == commands.end())
    return Arguments(fmt::sprintf(error::unknown_command, args[0]));
  
  return Arguments();
}

namespace error
{
  static const std::string missing_command = "missing command";
  static const std::string unknown_command = "unknown command: %s";

}
