#include "args.h"

#include <algorithm>
#include <iostream>

#include "tbx/extra/fmt/format.h"

using namespace args;

Parser::Parser()
{
  using cmd = cli::Command;
  
  program = "rzip";
  version = "0.1a";
  copyright = "";
  
  commands = {
    { cmd::LIST_CONTENTS, "l", "list contents of archive" },
    { cmd::TEST_INTEGRITY, "t", "test integrity of archive" },
    { cmd::EXTRACT, "x", "extract file with full paths" },
    { cmd::CREATE_SINGLE_STREAM, "s", "create single stream archive" }
  };

  std::sort(commands.begin(), commands.end(), [](const CommandArgument& c1, const CommandArgument& c2) { return c1.mnemonic < c2.mnemonic; });
}

void Parser::print()
{
  std::cout << std::endl;
  std::cout << program << " " << version << copyright << std::endl << std::endl;
  std::cout << "Usage:  " << program << " <command> [<switches>...] <archive_name> [<file_names>...]" << std::endl << std::endl;
  
  std::cout << "<Commands>" << std::endl;
  for (const auto& command : commands)
    std::cout << "  " << command.mnemonic << " : " << command.help << std::endl;
}

Arguments Parser::parse(const std::vector<std::string>& args)
{
  if (args.empty())
    return Arguments(error::missing_command);
  
  const auto command = std::find(commands.begin(), commands.end(), args[0]);
  
  if (command == commands.end())
    return Arguments(fmt::sprintf(error::unknown_command, args[0]));

  /* discard command */
  auto it = args.begin() + 1;
  while (it != args.end())
  {
    const std::string& arg = *it;
    
    if (arg == "--")
    {
      ++it;
      break;
    }
    else if (arg[0] != '-')
      break;
    else
    {
      /* parse switch */
    }
  }
  
  path archiveName;
  std::vector<path> fileNames;
  
  if (it != args.end())
    archiveName = *(it++);
  
  while (it != args.end())
    fileNames.push_back(*(it++));
    
  
  return Arguments(command->command, archiveName, fileNames);
}

namespace error
{
  static const std::string missing_command = "missing command";
  static const std::string unknown_command = "unknown command: %s";

}
