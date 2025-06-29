#include "catch2/catch_all.hpp"

#include "tbx/hash/hash.h"
#include "test_support.h"

#pragma mark hashes/crypto
TEST_CASE("algorithms.ChecksumCRC32::first")
{
  std::string testString = "The quick brown fox jumps over the lazy dog";
  hash::crc32_t crc = hash::crc32_digester::compute(testString.data(), testString.length());
  REQUIRE(crc == 0x414FA339);
}

TEST_CASE("algorithms.ChecksumCRC32::second")
{
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

TEST_CASE("algorithms.ChecksumMD5::first")
{
    std::string testString = "The quick brown fox jumps over the lazy dog";
    std::string md5 = hash::md5_digester::compute(testString.data(), testString.length());
    REQUIRE(md5 == "9e107d9d372bb6826bd81d3542a419d6");
}
  
TEST_CASE("algorithms.ChecksumMD5::second")
{
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

TEST_CASE("algorithms.ChecksumSHA1::first")
{
    std::string testString = "The quick brown fox jumps over the lazy dog";
    std::string sha1 = hash::sha1_digester::compute(testString.data(), testString.length());
    REQUIRE(sha1 == "2fd4e1c67a2d28fced849ee1bb76e7391b93eb12");
}
  
TEST_CASE("algorithms.ChecksumSHA1::second")
{
  std::string testString = "Lorem ipsum dolor sit amet, consectetur adipiscing"
  " elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua."
  " Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi"
  " ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit"
  " in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur"
  " sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt"
  " mollit anim id est laborum";
  std::string sha1 = hash::sha1_digester::compute(testString.data(), testString.length());
  REQUIRE(sha1 == "a851751e1e14c39a78f0a4b8debf69dba0b2ae0d");
}
  
TEST_CASE("algorithms.ChecksumSHA1::single-byte")
{
    byte data[1] = { 'a' };
    std::string sha1 = hash::sha1_digester::compute(data, 1);
    REQUIRE(sha1 == "86f7e437faa5a7fce15d1ddcb9eaeaea377667b8");
}
  
TEST_CASE("algorithms.ChecksumSHA1::single-block")
{
    std::string testString = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    REQUIRE(testString.length() == 64);
    std::string sha1 = hash::sha1_digester::compute(testString.data(), testString.length());
    REQUIRE(sha1 == "ce4303f6b22257d9c9cf314ef1dee4707c6e1c13");
}
  
TEST_CASE("algorithms.ChecksumSHA1::single-block-less-one-byte")
{
    std::string testString = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcde";
    REQUIRE(testString.length() == 63);
    std::string sha1 = hash::sha1_digester::compute(testString.data(), testString.length());
    REQUIRE(sha1 == "ef717286343f6da3f4e6f68c6de02a5148a801c4");
}
  
TEST_CASE("algorithms.ChecksumSHA1::partial-updates")
{
  std::string testString = "Lorem ipsum dolor sit amet, consectetur adipiscing"
  " elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua."
  " Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi"
  " ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit"
  " in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur"
  " sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt"
  " mollit anim id est laborum";
  const size_t length = testString.length();
  size_t available = testString.length();
  
  hash::sha1_digester digester;
  while (available > 0)
  {
    size_t current = testing::random((u32)std::min(available+1, (size_t)64UL));
    digester.update(testString.data() + (length - available), current);
    available -= current;
  }
  
  std::string sha1 = digester.get();
  REQUIRE(sha1 == "a851751e1e14c39a78f0a4b8debf69dba0b2ae0d");
}
