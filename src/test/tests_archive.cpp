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
    data.addRaw("entry.bin", testing::randomDataSource(testing::random(512) + 512));
  }

  SECTION("two entries") {
    for (size_t i = 0; i < 2; ++i)
    {
      data.addRaw(fmt::format("entry{}.bin", i), testing::randomDataSource(testing::random(512) + 512));
    }
  }

  SECTION("ten entries") {
    for (size_t i = 0; i < 10; ++i)
    {
      data.addRaw(fmt::format("entry{}.bin", i), testing::randomDataSource(testing::random(512) + 512));
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

TEST_CASE("features.Archive.multipleEntriesPerStreamWithNoFilter") {
  ArchiveFactory::Data data;
  data.entries.push_back({ "foobar1.bin", testing::randomDataSource(256) });
  data.entries.push_back({ "foobar2.bin", testing::randomDataSource(512) });
  data.streams.push_back({ { 0, 1 }, { } });

  testing::ArchiveTester::verify(data);
}

TEST_CASE("features.Archive.multipleEntriesPerStreamWithDeflate") {
  ArchiveFactory::Data data;
  data.entries.push_back({ "foobar1.bin", testing::randomDataSource(256) });
  data.entries.push_back({ "foobar2.bin", testing::randomDataSource(512) });
  data.streams.push_back({ { 0, 1 }, { new builders::deflate_builder(256) } });

  testing::ArchiveTester::verify(data);
}

TEST_CASE("features.Archive.singleEntryWithFilters")
{
  ArchiveFactory::Data data;

  SECTION("no filters") {
    data.entries.push_back({ "foobar1.bin", testing::randomDataSource(256) });
    data.streams.push_back({ { 0 }, { } });

  }

  SECTION("xor filter on entry") {
    data.entries.push_back({ "foobar1.bin", testing::randomDataSource(256), { new builders::xor_builder(256, "foobar") } });
    data.streams.push_back({ { 0 }, { } });
  }

  SECTION("xor filter on stream") {
    data.entries.push_back({ "foobar1.bin", testing::randomDataSource(256), { } });
    data.streams.push_back({ { 0 }, { new builders::xor_builder(256, "foobar") } });
  }

  SECTION("zlib deflate filter on entry") {
    data.entries.push_back({ "foobar1.bin", testing::randomCompressibleDataSource(16_kb), { new builders::deflate_builder(16_kb) } });
    data.streams.push_back({ { 0 }, { } });
  }

  SECTION("double filter on entry (deflate + xor)") {
    data.entries.push_back({ "foobar1.bin", testing::randomCompressibleDataSource(16_kb), { new builders::deflate_builder(16_kb), new builders::xor_builder(16_kb, "foobar") } });
    data.streams.push_back({ { 0 }, { } });
  }

  SECTION("double filter on entry (xor + deflate)") {
    data.entries.push_back({ "foobar1.bin", testing::randomCompressibleDataSource(16_kb), { new builders::xor_builder(16_kb, "foobar"), new builders::deflate_builder(16_kb) } });
    data.streams.push_back({ { 0 }, { } });
  }

  SECTION("double xor on entry and then on stream") {
    data.entries.push_back({ "entry.bin", testing::randomDataSource(256), { new builders::xor_builder(32, "foobar") } });
    data.streams.push_back({ { 0 }, { new builders::xor_builder(32, "lorem") } });
  }

  SECTION("xor on entry and lzma on stream") {
    data.entries.push_back({ "entry.bin", testing::randomCompressibleDataSource(16_kb), { new builders::xor_builder(32, "foobar") } });
    data.streams.push_back({ { 0 }, { new builders::lzma_builder(32) } });
  }

  SECTION("lzma on entry and xor on stream") {
    data.entries.push_back({ "entry.bin", testing::randomCompressibleDataSource(16_kb), { new builders::lzma_builder(32) } });
    data.streams.push_back({ { 0 }, { new builders::xor_builder(32, "foobar") } });
  }

  SECTION("lzma on entry and deflate on stream") {
    data.entries.push_back({ "entry.bin", testing::randomCompressibleDataSource(16_kb), { new builders::lzma_builder(32) } });
    data.streams.push_back({ { 0 }, { new builders::deflate_builder(32) } });
  }

  testing::ArchiveTester::verify(data);
}