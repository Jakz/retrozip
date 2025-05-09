#include "logger.h"

#include "tbx/extra/termcolor/termcolor.h"

#include <cassert>
#include <iostream>

void Logger::log(LogLevel level, std::string_view message)
{
  if (false && level == LogLevel::Trace)
    return;
  
  switch (level)
  {
    case LogLevel::Error:
    case LogLevel::Fatal:
      std::cout << termcolor::red;
      break;
    case LogLevel::Warning:
      std::cout << termcolor::yellow;
      break;
    case LogLevel::Info:
      std::cout << termcolor::bright_white;
      break;
    case LogLevel::Debug:
      std::cout << termcolor::white;
      break;
    case LogLevel::Trace:
      std::cout << termcolor::cyan;
      break;
    default:
      assert(false);
  }

  std::cout << message << std::endl;
  std::cout << termcolor::reset;
}
