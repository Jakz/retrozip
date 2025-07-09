#include "catch2/catch_all.hpp"

#include "test/test_support.h"

#include "box/archive.h"
#include "tbx/streams/memory_buffer.h"
#include "tbx/streams/data_source.h"
#include "box/header.h"

using eds = enriched_data_sink;


TEST_CASE("classes.MetadataEntry.sizeInBytes(empty)")
{
  box::MetadataEntry entry;
  REQUIRE(entry.sizeInBytes() == (sizeof(box::MetadataType) + eds::sizeofLEB128(0) + eds::sizeofLEB128(0)));
}

TEST_CASE("classes.MetadataEntry.sizeInBytes(name)")
{
  box::MetadataEntry entry(box::KnownMetadata::Name, "title");
  REQUIRE(entry.sizeInBytes() == (sizeof(box::MetadataType) + eds::sizeofLEB128(strlen("title")) + strlen("title")));
}

TEST_CASE("classes.MetadataEntry.sizeInBytes(string+string)")
{
  auto entry = box::MetadataEntry("foo", "bar");

  REQUIRE(entry.sizeInBytes() == (
          sizeof(box::MetadataType) +
          eds::sizeofLEB128(strlen("foo")) + strlen("foo") +
          eds::sizeofLEB128(strlen("bar")) + strlen("bar")
    )
  );
}

TEST_CASE("classes.MetadataEntry.sizeInBytes(uid+string)")
{
  auto entry = box::MetadataEntry(12345, "bar");

  REQUIRE(entry.sizeInBytes() == (
    sizeof(box::MetadataType) +
    eds::sizeofLEB128(12345) +
    eds::sizeofLEB128(strlen("bar")) + strlen("bar")
    )
  );
}

TEST_CASE("classes.MetadataEntry.serialize(string+string)")
{
  memory_buffer buffer(256);
  auto entry = box::MetadataEntry("foo", "bar");

  entry.serialize(&buffer);

  box::MetadataEntry entry2;
  buffer.seek(0);
  entry2.unserialize(&buffer);

  REQUIRE(entry == entry2);
}

TEST_CASE("classes.MetadataEntry.serialize(name)")
{
  memory_buffer buffer(256);
  auto entry = box::MetadataEntry(box::KnownMetadata::Name, "bar");

  entry.serialize(&buffer);

  box::MetadataEntry entry2;
  buffer.seek(0);
  entry2.unserialize(&buffer);

  REQUIRE(entry == entry2);
}

TEST_CASE("classes.MetadataEntry.serialize(uid+string)")
{
  memory_buffer buffer(256);
  auto entry = box::MetadataEntry(12345, "bar");

  entry.serialize(&buffer);

  box::MetadataEntry entry2;
  buffer.seek(0);
  entry2.unserialize(&buffer);

  REQUIRE(entry == entry2);
}

TEST_CASE("classes.MetadataEntry.entryWithSingleMetadata")
{
  ArchiveFactory::Data data;

  data.addRaw("entry.bin", testing::randomDataSource(testing::random(32) + 32));
  data.entries.back().metadata.add("foo", "bar");

  Archive archive = Archive::ofData(data);

  REQUIRE(archive.entries()[0].metadata().size() == 2);
  REQUIRE(archive.entries()[0].metadata(1) == box::MetadataEntry("foo", "bar"));

  memory_buffer output;
  archive.write(output);
  output.rewind();

  Archive verify;
  verify.read(output);

  REQUIRE(verify.entries()[0].metadata().size() == 2);
  REQUIRE(verify.entries()[0].metadata(1) == box::MetadataEntry("foo", "bar"));

  verify.options().bufferSize = 16_kb;
  testing::ArchiveTester::verify(data, verify, output);

  testing::ArchiveTester::release(data);
}

TEST_CASE("classes.Metadata.serializeEmpty")
{
  memory_buffer buffer(256);
  Metadata metadata;

  metadata.serialize(&buffer);

  REQUIRE(buffer.size() == enriched_data_sink::sizeofLEB128(0));

  buffer.rewind();
  metadata.unserialize(&buffer);

  REQUIRE(metadata.size() == 0);
}


TEST_CASE("classes.Metadata.serializeSingle")
{
  memory_buffer buffer(256);
  Metadata metadata;

  metadata.add("foo", "bar");
  metadata.serialize(&buffer);

  REQUIRE(buffer.size() == metadata.sizeInBytes());

  buffer.rewind();
  metadata.unserialize(&buffer);

  REQUIRE(metadata.size() == 1);
  REQUIRE(metadata[0] == box::MetadataEntry("foo", "bar"));
}

TEST_CASE("classes.Metadata.serializeMultiple")
{
  memory_buffer buffer(256);
  Metadata metadata;

  metadata.add("foo", "bar");
  metadata.add(box::KnownMetadata::Name, "title");
  metadata.add(12345, "baz");
  metadata.serialize(&buffer);

  REQUIRE(buffer.size() == metadata.sizeInBytes());

  buffer.rewind();
  metadata.unserialize(&buffer);

  REQUIRE(metadata.size() == 3);
  REQUIRE(metadata[0] == box::MetadataEntry("foo", "bar"));
  REQUIRE(metadata[1] == box::MetadataEntry(box::KnownMetadata::Name, "title"));
  REQUIRE(metadata[2] == box::MetadataEntry(12345, "baz"));
}