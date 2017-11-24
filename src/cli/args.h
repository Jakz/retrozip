#include <string>

#include "libs/fmt/printf.h"

namespace args
{

  struct Command
  {
    std::string command;
    std::string help;
    bool operator==(const std::string& command) const { return this->command == command; }
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
    Command command;
    
    Arguments() { }
    Arguments(std::string& message) : error(message) { }
    Arguments(Error&& error) : error(error) { }
    
    operator bool() const { return !error.present; }
  };

  struct Parser
  {
    std::string program;
    std::string version;
    std::string copyright;
    std::vector<Command> commands;
    
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
