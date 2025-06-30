#include "catch2/catch_all.hpp"

#include "test/test_support.h"
#include "tbx/streams/memory_buffer.h"
#include "tbx/formats/patch/ips.h"


TEST_CASE("classes.PatchUps.writeVariableInt")
{
  struct Test {
    uint64_t value;
    size_t expectedSize;
  };

  std::array<Test, 8> tests = { {
    { 0x000012, 1 },
    { 0x00007f, 1 },
    { 0x000080, 2 },
    { 0x003FFF, 2 },
    { 0x004000, 2 },
    { 0x005000, 3 },
    { 0x1FFFFF, 3 },
    { 0x200000, 4 }
  } };

  for (const auto& test : tests)
  {
    memory_buffer buffer;

    patch::ups::Patch::writeVariableInt(&buffer, test.value);

    CAPTURE(test.value, test.expectedSize);
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

TEST_CASE("classes.PatchUps.patchEmpty")
{
  memory_buffer source = testing::randomStackDataSource(256);
  memory_buffer source2 = source;
  patch::ups::Patch patch;
  patch.generate(&source, &source2);

  memory_buffer buffer;
  patch.write(&buffer);
  buffer.rewind();

  patch::ups::Patch parsed;
  REQUIRE(parsed.load(&buffer) == patch::ups::Status::Ok);

  memory_buffer patched;
  source.rewind();
  parsed.apply(&source, &patched);

  REQUIRE(source2 == patched);
}


#ifdef _WIN32
#include <cstdio>
#include <sstream>
char tmp_filename[L_tmpnam];
FILE* tmp_file = NULL;
int original_stdout = -1;
void captureStdout()
{
  // Create a temp file
  tmpnam_s(tmp_filename);

  tmp_file = nullptr;
  freopen_s(&tmp_file, tmp_filename, "w+", stdout);
  if (!tmp_file)
    ;

  original_stdout = _dup(_fileno(stdout)); // save original stdout
}

std::string releaseStdout()
{
  fflush(stdout);
  fseek(tmp_file, 0, SEEK_SET);

  std::stringstream output;
  char buf[1024];
  while (fgets(buf, sizeof(buf), tmp_file)) {
    output << buf;
  }

  // Restore original stdout
  _dup2(original_stdout, _fileno(stdout));
  _close(original_stdout);
  fclose(tmp_file);
  remove(tmp_filename);

  return output.str();
}

#endif

TEST_CASE("classes.PatchUps.patchSingleByteDifferenceInTheMiddle")
{
  memory_buffer source = testing::randomStackDataSource(256);
  memory_buffer source2 = source;
  source[128] = 0xAB;
  source2[128] = 0x00;

  patch::ups::Patch patch;
  patch.generate(&source, &source2);

  memory_buffer buffer;
  patch.write(&buffer);
  buffer.rewind();

  patch::ups::Patch parsed;
  REQUIRE(parsed.load(&buffer) == patch::ups::Status::Ok);

  memory_buffer patched;
  source.rewind();
  parsed.apply(&source, &patched);

  REQUIRE(source2 == patched);
}
