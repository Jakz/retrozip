#define CATCH_CONFIG_MAIN
#include "libs/catch.h"

#include "hash/hash.h"

TEST_CASE("catch library correctly setup", "[setup]") {
  REQUIRE(true);
}

TEST_CASE("crc32", "[checksums]") {
  SECTION("crc32-test1") {
    std::string testString = "The quick brown fox jumps over the lazy dog";
    hash::crc32_t crc = hash::crc32_digester::compute(testString.data(), testString.length());
    REQUIRE(crc == 0x414FA339);
  }
  SECTION("crc32-test2", "[crc32]") {
    std::string testString = "Lorem ipsum dolor sit amet, consectetur adipiscing"
      " elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua."
      " Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi"
      " ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit"
      " in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur"
      " sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt"
      " mollit anim id est laborum";
    hash::crc32_t crc = hash::crc32_digester::compute(testString.data(), testString.length());
    REQUIRE(crc == 0x6F8F714A);
  }
}

TEST_CASE("md5", "[checksums]") {
  SECTION("md5-test1") {
    std::string testString = "The quick brown fox jumps over the lazy dog";
    std::string md5 = hash::md5_digester::compute(testString.data(), testString.length());
    REQUIRE(md5 == "9e107d9d372bb6826bd81d3542a419d6");
  }
  
  SECTION("md5-test2") {
    std::string testString = "Lorem ipsum dolor sit amet, consectetur adipiscing"
    " elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua."
    " Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi"
    " ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit"
    " in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur"
    " sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt"
    " mollit anim id est laborum";
    std::string md5 = hash::md5_digester::compute(testString.data(), testString.length());
    REQUIRE(md5 == "b69c72d396328f617dbf9ba3ebe7cefc");
  }
}
