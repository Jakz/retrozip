#pragma once

#include <string>
#include <string_view>

#include "tbx/extra/fmt/format.h"


#include "tbx/base/path.h"

template<>
struct fmt::formatter<path> : fmt::formatter<std::string_view> {
  template<typename FormatContext>
  auto format(const path& path, FormatContext& ctx) const {
    return fmt::formatter<std::string_view>::format(path.c_str(), ctx);
  }
};

class Logger
{
public:
  enum class Level { Trace, Debug, Info, Warning, Error, Fatal };

public:
  void log(Level level, std::string_view message);

  void log(Level level, std::string_view section, std::string_view message) { log(level, fmt::format("[{}] {}", section, message)); }
  template<typename... Args> void log(Level level, std::string_view section, std::string_view format, Args&&... args) { 
    std::string formatted = fmt::vformat(format, fmt::make_format_args(args...));
    log(level, section, formatted);
  }

  template<typename... Args> void trace(std::string_view section, std::string_view format, Args&&... args) { log(Level::Trace, section, format, std::forward<Args>(args)...); }
  template<typename... Args> void debug(std::string_view section, std::string_view format, Args&&... args) { log(Level::Debug, section, format, std::forward<Args>(args)...); }
  template<typename... Args> void info(std::string_view section, std::string_view format, Args&&... args) { log(Level::Info, section, format, std::forward<Args>(args)...); }
  template<typename... Args> void warning(std::string_view section, std::string_view format, Args&&... args) { log(Level::Warning, section, format, std::forward<Args>(args)...); }
  template<typename... Args> void error(std::string_view section, std::string_view format, Args&&... args) { log(Level::Error, section, format, std::forward<Args>(args)...); }
  template<typename... Args> void fatal(std::string_view section, std::string_view format, Args&&... args) { log(Level::Fatal, section, format, std::forward<Args>(args)...); }
};