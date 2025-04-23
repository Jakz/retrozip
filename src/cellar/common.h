#pragma once

#include "tbx/extra/fmt/format.h"
#include "tbx/base/path.h"

enum class LogLevel { Trace, Debug, Info, Warning, Error, Fatal };

template<>
struct fmt::formatter<path> : fmt::formatter<std::string_view> {
  template<typename FormatContext>
  auto format(const path& path, FormatContext& ctx) const {
    return fmt::formatter<std::string_view>::format(path.c_str(), ctx);
  }
};