#include "catch2/catch_all.hpp"

#include "test/test_support.h"

#include "box/archive.h"
#include "tbx/streams/memory_buffer.h"
#include "tbx/streams/data_source.h"
#include "box/header.h"

using eds = enriched_data_sink;


TEST_CASE("features.MetadataEntry.sizeInBytes(empty)")
{
  box::MetadataEntry entry;
  REQUIRE(entry.sizeInBytes() == (sizeof(box::MetadataType) + eds::sizeofLEB128(0) + eds::sizeofLEB128(0)));
}

TEST_CASE("features.MetadataEntry.sizeInBytes(string+string)")
{
  auto entry = box::MetadataEntry("foo", "bar");

  REQUIRE(entry.sizeInBytes() == (
          sizeof(box::MetadataType) +
          eds::sizeofLEB128(strlen("foo")) + strlen("foo") +
          eds::sizeofLEB128(strlen("bar")) + strlen("bar")
    )
  );
}

TEST_CASE("features.MetadataEntry.sizeInBytes(uid+string)")
{
  auto entry = box::MetadataEntry(123, "bar");

  REQUIRE(entry.sizeInBytes() == (
    sizeof(box::MetadataType) +
    eds::sizeofLEB128(123) +
    eds::sizeofLEB128(strlen("bar")) + strlen("bar")
    )
  );
}

TEST_CASE("features.MetadataEntry.serialize(string+string)")
{
  memory_buffer buffer(256);
  auto entry = box::MetadataEntry("foo", "bar");

  entry.serialize(&buffer);

  box::MetadataEntry entry2;
  buffer.seek(0);
  entry2.unserialize(&buffer);

  REQUIRE(entry == entry2);
}

TEST_CASE("features.MetadataEntry.serialize(uid+string)")
{
  memory_buffer buffer(256);
  auto entry = box::MetadataEntry(12345, "bar");

  entry.serialize(&buffer);

  box::MetadataEntry entry2;
  buffer.seek(0);
  entry2.unserialize(&buffer);

  REQUIRE(entry == entry2);
}

TEST_CASE("features.MetadataEntry.entryWithSingleMetadata")
{
  ArchiveFactory::Data data;

  ArchiveFactory::Entry entry = { "entry.bin", testing::randomDataSource(testing::random(32) + 32) };
  entry.metadata.emplace_back("foo", "bar");

  data.entries.push_back(entry);
  data.streams.push_back({ { 0 }, { } });

  Archive archive = Archive::ofData(data);

  REQUIRE(archive.entries()[0].metadata().size() == 1);
  REQUIRE(archive.entries()[0].metadata(0) == box::MetadataEntry("foo", "bar"));

  memory_buffer output;
  archive.write(output);
  output.rewind();

  Archive verify;
  verify.read(output);

  REQUIRE(verify.entries()[0].metadata().size() == 1);
  REQUIRE(verify.entries()[0].metadata(0) == box::MetadataEntry("foo", "bar"));

  verify.options().bufferSize = 16_kb;
  testing::ArchiveTester::verify(data, verify, output);

  testing::ArchiveTester::release(data);
}