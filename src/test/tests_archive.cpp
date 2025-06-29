#include "catch2/catch_all.hpp"

#include "tbx/hash/hash.h"
#include "test_support.h"


TEST_CASE("features.Archive.createEmpty")
{
  memory_buffer buffer;

  Archive source, result;

  source.options().checksum.calculateGlobalChecksum = true;
  source.options().checksum.digesterBuffer = 16_kb;

  source.write(buffer);
  result.read(buffer);

  REQUIRE(buffer.size() == sizeof(box::Header));
  REQUIRE(buffer.size() == result.header().fileLength);

  REQUIRE(result.sections().empty());

  REQUIRE(result.isValidMagicNumber());
  REQUIRE(result.header().hasFlag(box::HeaderFlag::INTEGRITY_CHECKSUM_ENABLED));
  REQUIRE(result.isValidGlobalChecksum(buffer));
}

TEST_CASE("features.Archive.singleEntryPerStreamWithNoFilter")
{
  ArchiveFactory::Data data;

  SECTION("single entry") {
    data.entries.push_back({ "entry.bin", testing::randomDataSource(testing::random(512) + 512) });
    data.streams.push_back({ { 0 }, { } });
  }

  SECTION("two entries") {
    for (size_t i = 0; i < 2; ++i)
    {
      data.entries.push_back({ fmt::format("entry{}.bin", i), testing::randomDataSource(testing::random(512) + 512) });
      data.streams.push_back({ { static_cast<int>(i) }, { } });
    }
  }

  SECTION("ten entries") {
    for (size_t i = 0; i < 10; ++i)
    {
      data.entries.push_back({ fmt::format("entry{}.bin", i), testing::randomDataSource(testing::random(512) + 512) });
      data.streams.push_back({ { static_cast<int>(i) }, { } });
    }
  }

  Archive archive = Archive::ofData(data);
  memory_buffer output;
  archive.write(output);
  output.rewind();

  Archive verify;
  verify.read(output);
  verify.options().bufferSize = 16_kb;
  testing::ArchiveTester::verify(data, verify, output);

  testing::ArchiveTester::release(data);
}
