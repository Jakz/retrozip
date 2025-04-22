#include "logger.h"

#include "tbx/extra/termcolor/termcolor.h"

#include <cassert>
#include <iostream>

void Logger::log(Logger::Level level, std::string_view message)
{
  if (level == Level::Trace)
    return;
  
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
    case Level::Trace:
      std::cout << termcolor::cyan;
      break;
    default:
      assert(false);
  }

  std::cout << message << std::endl;
  std::cout << termcolor::reset;
}
