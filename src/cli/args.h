#include <string>

#include "base/path.h"

#include "libs/fmt/printf.h"

namespace cli
{
  enum class Command
  {
    LIST_CONTENTS = 0,
    TEST_INTEGRITY,
    EXTRACT,
    CREATE_SINGLE_STREAM
  };
  
  enum class Switch
  {
    
  };
  
  class GenericSwitch
  {
  private:
    Switch _switch;
    
  protected:
    GenericSwitch(Switch _switch) : _switch(_switch) { }
    
  };
  
  class BooleanSwitch
  {
    
    
  };
}

namespace args
{

  struct CommandArgument
  {
    cli::Command command;
    std::string mnemonic;
    std::string help;
    bool operator==(const std::string& mnemonic) const { return this->mnemonic == mnemonic; }
  };

  struct Error
  {
    bool present;
    std::string message;
    
    Error() : present(false) { }
    Error(const std::string& message) : present(true), message(message) { }
  };

  struct Arguments
  {
    Error error;
    cli::Command command;
    path archiveName;
    std::vector<path> fileNames;
    
    Arguments(cli::Command command, const path& archiveName, const std::vector<path>& fileNames) : command(command), archiveName(archiveName), fileNames(fileNames) { }
    Arguments(std::string& message) : error(message) { }
    Arguments(Error&& error) : error(error) { }
    
    operator bool() const { return !error.present; }
  };

  struct Parser
  {
    std::string program;
    std::string version;
    std::string copyright;
    std::vector<CommandArgument> commands;
    
    Parser();
    Arguments parse(const std::vector<std::string>& args);
    void print();
  };

  namespace error
  {
    const static std::string missing_command;
    const static std::string unknown_command;
  }
}
