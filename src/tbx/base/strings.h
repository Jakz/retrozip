#pragma once

#include <string>
#include <algorithm>
#include <vector>

namespace strings
{
  std::string humanReadableSize(size_t bytes, bool si, uint32_t p = 1);
  bool isPrefixOf(const std::string& string, const std::string& prefix);
  std::string tolower(const std::string& text);
  inline bool contains(const std::string& text, const std::string& substring) { return text.find(substring) != std::string::npos; }
  inline bool containsIgnoreCase(const std::string& text, const std::string& substring) { return contains(tolower(text), tolower(substring)); }	
  inline bool caseInsensitiveEqual(const std::string& t1, const std::string& t2) { return tolower(t1) == tolower(t2); }

  std::vector<uint8_t> toByteArray(const std::string& string);
  std::string fromByteArray(const uint8_t* data, size_t length);
  inline std::string fromByteArray(const std::vector<uint8_t>& data) { return fromByteArray(data.data(), data.size()); }

  std::string fileNameFromPath(const std::string& path);

  std::vector<std::string> splitText(const std::string& text, int32_t length, const std::string& delimiter);
}