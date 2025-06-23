#include "catch2/catch_all.hpp"

#include "box/header.h"


TEST_CASE("features.MetadataEntry.sizeInBytes")
{
  {
    box::MetadataEntry entry;
    REQUIRE(entry.sizeInBytes() == 0);
  }
  {
    auto entry = box::MetadataEntry("foo", "bar");
    REQUIRE(entry.sizeInBytes() ==
            sizeof(box::MetadataEntry::key_len_t) +
            entry.key().size() +
            entry.data().size() +
            sizeof(box::MetadataEntry::data_len_t) +
            sizeof(box::MetadataType)
            );
  }
}
