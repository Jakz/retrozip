#include "catch2/catch_all.hpp"

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