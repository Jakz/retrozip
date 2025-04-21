#pragma once

#include <string>
#include <string_view>

#include "tbx/extra/fmt/format.h"

class Logger
{
public:
  enum class Level { Trace, Debug, Info, Warning, Error, Fatal };

public:
  void log(Level level, const std::string& message);

  void log(Level level, std::string_view section, const std::string& message) { log(level, fmt::format("[{}] {}", section, message)); }
  template<typename... Args> void log(Level level, std::string_view section, const std::string& format, Args&&... args) { log(level, fmt::format("[{}] {}", section, fmt::format(format, std::forward<Args>(args)...))); }

  template<typename... Args> void trace(std::string_view section, const std::string& format, Args&&... args) { log(Level::Trace, fmt::format("[{}] {}", section, fmt::format(format, std::forward<Args>(args)...))); }
  template<typename... Args> void debug(std::string_view section, const std::string& format, Args&&... args) { log(Level::Debug, fmt::format("[{}] {}", section, fmt::format(format, std::forward<Args>(args)...))); }
  template<typename... Args> void info(std::string_view section, const std::string& format, Args&&... args) { log(Level::Info, fmt::format("[{}] {}", section, fmt::format(format, std::forward<Args>(args)...))); }
  template<typename... Args> void warning(std::string_view section, const std::string& format, Args&&... args) { log(Level::Warning, fmt::format("[{}] {}", section, fmt::format(format, std::forward<Args>(args)...))); }
  template<typename... Args> void error(std::string_view section, const std::string& format, Args&&... args) { log(Level::Error, fmt::format("[{}] {}", section, fmt::format(format, std::forward<Args>(args)...))); }
  template<typename... Args> void fatal(std::string_view section, const std::string& format, Args&&... args) { log(Level::Fatal, fmt::format("[{}] {}", section, fmt::format(format, std::forward<Args>(args)...))); }
};