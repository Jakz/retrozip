#include "logger.h"

#include "tbx/extra/termcolor/termcolor.h"

#include <cassert>
#include <iostream>

void Logger::log(Logger::Level level, const std::string& message)
{
  switch (level)
  {
    case Level::Error:
    case Level::Fatal:
      std::cout << termcolor::red;
      break;
    case Level::Warning:
      std::cout << termcolor::yellow;
      break;
    case Level::Info:
      std::cout << termcolor::bright_white;
      break;
    case Level::Debug:
      std::cout << termcolor::white;
      break;
    default:
      assert(false);
  }

  std::cout << message << std::endl;
  std::cout << termcolor::reset;
}
