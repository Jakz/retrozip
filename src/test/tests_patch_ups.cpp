#include "tbx/formats/patch/ips.h"

#include "tbx/streams/memory_buffer.h"
#include "tbx/formats/patch/ips.h"

#include "catch2/catch_all.hpp"

TEST_CASE("classes.PatchUps.writeVariableInt")
{
  struct Test {
    uint64_t value;
    size_t expectedSize;
  };

  std::array<Test, 7> tests = { {
    { 0x000012, 1 },
    { 0x00007f, 1 },
    { 0x000080, 2 },
    { 0x003FFF, 2 },
    { 0x004000, 3 },
    { 0x1FFFFF, 3 },
    { 0x200000, 4 }
  } };

  memory_buffer buffer;

  for (const auto& test : tests)
  {
    buffer.seek(0);
    patch::ups::Patch::writeVariableInt(&buffer, test.value);
    REQUIRE(buffer.size() == test.expectedSize);
  }
}

TEST_CASE("classes.PatchUps.readVariableInt")
{
  std::array<uint64_t, 7> values = {
   0x000012,
   0x00007f,
   0x000080,
   0x003FFF,
   0x004000,
   0x1FFFFF,
   0x200000
  };

  memory_buffer buffer;

  for (const auto& value1 : values)
  {
    buffer.seek(0);
    patch::ups::Patch::writeVariableInt(&buffer, value1);
    buffer.seek(0);
    uint64_t value2 = patch::ups::Patch::readVariableInt(&buffer);
    REQUIRE(value1 == value2);
  }
}
