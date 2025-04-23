#pragma once

#include <string>
#include <string_view>

#include "common.h"
#include "tbx/extra/fmt/format.h"

#include "tbx/base/path.h"

class Logger
{
public:

public:
  Logger() { }

  void log(LogLevel level, std::string_view message);

  void log(LogLevel level, std::string_view section, std::string_view message) { log(level, fmt::format("[{}] {}", section, message)); }
  template<typename... Args> void log(LogLevel level, std::string_view section, std::string_view format, Args&&... args) {
    std::string formatted = fmt::vformat(format, fmt::make_format_args(args...));
    log(level, section, formatted);
  }

  /*template<typename... Args> void trace(std::string_view section, std::string_view format, Args&&... args) { log(LogLevel::Trace, section, format, std::forward<Args>(args)...); }
  template<typename... Args> void debug(std::string_view section, std::string_view format, Args&&... args) { log(LogLevel::Debug, section, format, std::forward<Args>(args)...); }
  template<typename... Args> void info(std::string_view section, std::string_view format, Args&&... args) { log(LogLevel::Info, section, format, std::forward<Args>(args)...); }
  template<typename... Args> void warning(std::string_view section, std::string_view format, Args&&... args) { log(LogLevel::Warning, section, format, std::forward<Args>(args)...); }
  template<typename... Args> void error(std::string_view section, std::string_view format, Args&&... args) { log(LogLevel::Error, section, format, std::forward<Args>(args)...); }
  template<typename... Args> void fatal(std::string_view section, std::string_view format, Args&&... args) { log(LogLevel::Fatal, section, format, std::forward<Args>(args)...); }*/
};
