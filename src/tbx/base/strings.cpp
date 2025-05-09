#include "strings.h"

#include "tbx/extra/fmt/format.h"
#include <cmath>
#include <cassert>

std::string strings::humanReadableSize(size_t bytes, bool si, uint32_t p) {
  static constexpr char pre[][7] = { "kMGTPE", "KMGTPE" };


  int unit = si ? 1000 : 1024;
  if (bytes < unit) return std::to_string(bytes) + "B";
  int exp = std::log(bytes) / std::log(unit);

  return fmt::format("{:.{}f}{}{}B", bytes / std::pow(unit, exp), p, pre[si ? 1 : 0][exp - 1], si ? "" : "i");
}

bool strings::isPrefixOf(const std::string& string, const std::string& prefix)
{
  return std::mismatch(prefix.begin(), prefix.end(), string.begin()).first == prefix.end();
}

std::string strings::tolower(const std::string& text)
{
  std::string lname;
  lname.resize(text.size());
  /* use std::transform */
  for (size_t i = 0; i < text.size(); ++i)
    lname[i] = std::tolower(text[i]);
  return lname;
}

std::vector<uint8_t> strings::toByteArray(const std::string& string)
{
  const size_t length = string.length();

  assert(length % 2 == 0);

  std::vector<uint8_t> array = std::vector<uint8_t>(length / 2, 0);

  for (size_t i = 0; i < length; ++i)
  {
    uint32_t shift = i % 2 == 0 ? 4 : 0;
    size_t position = i / 2;

    char c = string[i];
    uint8_t v = 0;

    if (c >= '0' && c <= '9') v = c - '0';
    else if (c >= 'a' && c <= 'f') v = c - 'a' + 0xa;
    else if (c >= 'A' && c <= 'F') v = c - 'A' + 0xa;
    else assert(false);

    array[position] |= v << shift;
  }

  return array;
}

std::string strings::fromByteArray(const uint8_t* data, size_t length)
{
  constexpr bool uppercase = false;

  std::vector<char> buf(length * 2 + 1);

  for (size_t i = 0; i < length; i++)
    sprintf(buf.data() + i * 2, uppercase ? "%02X" : "%02x", data[i]);

  buf[length * 2] = '\0';
  return buf.data();
}

std::string strings::fileNameFromPath(const std::string& path)
{
  size_t lastSeparator = path.find_last_of('/');
  if (lastSeparator != std::string::npos)
    return path.substr(lastSeparator + 1);
  else
    return path;
}


std::vector<std::string> strings::splitText(const std::string& text, int32_t length, const std::string& delimiter)
{
  const bool keepEmpty = false;

  std::vector<std::string> strings;

  std::string::size_type pos = 0;
  std::string::size_type prev = 0;
  while ((pos = text.find(delimiter, prev)) != std::string::npos)
  {
    std::string token = text.substr(prev, pos - prev);

    if (!token.empty() || keepEmpty)
      strings.push_back(text.substr(prev, pos - prev));

    prev = pos + delimiter.length();
  }

  std::string lastToken = text.substr(prev);

  if (!lastToken.empty() || keepEmpty)
    strings.push_back(text.substr(prev));

  if (length != -1)
  {
    for (size_t i = 0; i < strings.size(); ++i)
    {
      std::string& ith = strings[i];

      if (ith.length() > length)
      {
        std::string fs = ith.substr(0, length);
        std::string fh = ith.substr(length);

        strings[i] = fs;
        strings.insert(strings.begin() + i + 1, fh);
      }
    }
  }

  return strings;
}


