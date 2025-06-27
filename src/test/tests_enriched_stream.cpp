#include "catch2/catch_all.hpp"

#include "tbx/streams/memory_buffer.h"
#include "tbx/streams/data_source.h"

TEST_CASE("classes.EnrichedDataSink.writeLEB128")
{
  memory_buffer buffer(256);
  enriched_data_sink sink(&buffer);
  enriched_data_source source(&buffer);

  uint64_t val = GENERATE_COPY(
    0ULL, 1, 63, 127, 128, 255, 1024, 16384, 624485, 1ULL << 32, std::numeric_limits<uint64_t>::max()
  );

  sink.writeLEB128(val);
  buffer.seek(0);
  uint64_t val2 = source.readLEB128();
  REQUIRE(val2 == val);
}
