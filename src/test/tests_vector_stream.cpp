#include "tbx/streams/vector_stream.h"

#define CATCH_CONFIG_MAIN
#include "catch2/catch_all.hpp"

TEST_CASE("classes.WeakVectorSink.write")
{
  std::vector<uint8_t> data;
  weak_vector_sink sink(data);
  
  std::string foobar = "foobar";
  sink.write((const uint8_t*)foobar.data(), foobar.length() + 1);

  REQUIRE(data.size() == foobar.length() + 1);
  REQUIRE(std::memcmp(data.data(), foobar.data(), foobar.length() + 1) == 0);
}

TEST_CASE("classes.WeakVectorSource.read")
{
  std::vector<uint8_t> data;
  weak_vector_sink sink(data);

  std::string foobar = "foobar";
  sink.write((const uint8_t*)foobar.data(), foobar.length() + 1);

  weak_data_source source(data);
  std::string readData = std::string(foobar.length(), '\0');
  source.read((uint8_t*)readData.data(), foobar.length() + 1);
  
  REQUIRE(readData == foobar);
}