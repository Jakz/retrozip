#define CATCH_CONFIG_MAIN
#include "libs/catch.h"

#include "core/memory_buffer.h"
#include "hash/hash.h"

TEST_CASE("catch library correctly setup", "[setup]") {
  REQUIRE(true);
}

TEST_CASE("u32 biendian", "[endianness]") {
  SECTION("same endianness") {
    u32 x = 0x12345678;
    u32se se = 0x12345678;
    
    REQUIRE(x == se.operator u32());
    REQUIRE(memcmp(&x, &se, sizeof(u32)) == 0);
  }
  
  SECTION("reversed endianness") {
    u32 x = 0x12345678;
    u32de de = 0x12345678;
    
    const byte* p1 = reinterpret_cast<const byte*>(&x);
    const byte* p2 = reinterpret_cast<const byte*>(&de);
    
    REQUIRE(p1[0] == p2[3]);
    REQUIRE(p1[1] == p2[2]);
    REQUIRE(p1[2] == p2[1]);
    REQUIRE(p1[3] == p2[0]);
  }
}

#define WRITE_RANDOM_DATA(dest, name, length) byte name[(length)]; randomize(name, (length)); dest.write(name, 1, (length));
void randomize(byte* data, size_t len) { for (size_t i = 0; i < len; ++i) { data[i] = rand()%256; } }
#define READ_DATA(dest, name, length, res) byte name[(length)]; size_t res = dest.read(name, 1, (length));

TEST_CASE("memory buffer", "[support]") {
  SECTION("write") {
    SECTION("write with realloc") {
      constexpr size_t LEN = 64;
      memory_buffer b(0);
      
      WRITE_RANDOM_DATA(b, temp, LEN);
      
      REQUIRE(b.capacity() == LEN);
      REQUIRE(b.size() == LEN);
      REQUIRE(b.position() == LEN);
      REQUIRE(memcmp(b.raw(), temp, LEN) == 0);
    }
    
    SECTION("write without realloc") {
      constexpr size_t LEN = 64, CAP = 128;
      memory_buffer b(CAP);
      
      WRITE_RANDOM_DATA(b, temp, LEN);
      
      REQUIRE(b.capacity() == CAP);
      REQUIRE(b.size() == LEN);
      REQUIRE(b.position() == LEN);
      REQUIRE(memcmp(b.raw(), temp, LEN) == 0);
    }
    
    SECTION("two writes with realloc")
    {
      constexpr size_t LEN1 = 64, LEN2 = 32;
      memory_buffer b(0);
      
      WRITE_RANDOM_DATA(b, temp1, LEN1);

      REQUIRE(b.capacity() == LEN1);
      REQUIRE(b.size() == LEN1);
      REQUIRE(b.position() == LEN1);
      REQUIRE(memcmp(b.raw(), temp1, LEN1) == 0);

      WRITE_RANDOM_DATA(b, temp2, LEN2);
      
      REQUIRE(b.capacity() == LEN1 + LEN2);
      REQUIRE(b.size() == LEN1 + LEN2);
      REQUIRE(b.position() == LEN1 + LEN2);
      REQUIRE(memcmp(b.raw(), temp1, LEN1) == 0);
      REQUIRE(memcmp(b.raw()+LEN1, temp2, LEN2) == 0);
    }
    
    SECTION("overwrite with seek from start")
    {
      constexpr size_t LEN1 = 64, LEN2 = 32, OFF = 16;
      memory_buffer b(0);
      
      WRITE_RANDOM_DATA(b, temp1, LEN1);
      b.seek(OFF, Seek::SET);
      REQUIRE(b.position() == OFF);
      WRITE_RANDOM_DATA(b, temp2, LEN2);
      
      REQUIRE(b.capacity() == LEN1);
      REQUIRE(b.size() == LEN1);
      REQUIRE(b.position() == OFF + LEN2);
      REQUIRE(memcmp(b.raw(), temp1, OFF) == 0);
      REQUIRE(memcmp(b.raw() + OFF, temp2, LEN2) == 0);
      REQUIRE(memcmp(b.raw() + OFF + LEN2, temp1 + OFF + LEN2, LEN1 - OFF - LEN2) == 0);
    }
    
    SECTION("overwrite with seek from current")
    {
      /* 48 temp1 | 32 temp2 */
      constexpr size_t LEN1 = 64, LEN2 = 32, OFF = -16;
      memory_buffer b(0);
      
      WRITE_RANDOM_DATA(b, temp1, LEN1);
      b.seek(OFF, Seek::CUR);
      REQUIRE(b.position() == LEN1 + OFF);
      WRITE_RANDOM_DATA(b, temp2, LEN2);
      
      REQUIRE(b.capacity() == LEN1 + OFF + LEN2);
      REQUIRE(b.size() == LEN1 + OFF + LEN2);
      REQUIRE(b.position() == LEN1 + OFF + LEN2);
      
      REQUIRE(memcmp(b.raw(), temp1, LEN1 + OFF) == 0);
      REQUIRE(memcmp(b.raw() + LEN1 + OFF, temp2, LEN2) == 0);
    }
    
    SECTION("overwrite with negative seek from end")
    {
      /* 48 temp1 | 32 temp2 */
      constexpr size_t LEN1 = 64, LEN2 = 32, OFF = -16;
      memory_buffer b(0);
      
      WRITE_RANDOM_DATA(b, temp1, LEN1);
      b.seek(OFF, Seek::END);
      REQUIRE(b.position() == LEN1 + OFF);
      WRITE_RANDOM_DATA(b, temp2, LEN2);
      
      REQUIRE(b.capacity() == LEN1 + OFF + LEN2);
      REQUIRE(b.size() == LEN1 + OFF + LEN2);
      REQUIRE(b.position() == LEN1 + OFF + LEN2);
      
      REQUIRE(std::equal(b.raw(), b.raw() + LEN1 + OFF, temp1));
      REQUIRE(std::equal(b.raw() + LEN1 + OFF, b.raw() + LEN1 + OFF + LEN2, temp2));
    }
    
    SECTION("seek before start should revert to 0")
    {
      constexpr size_t LEN = 64, OFF = -128;
      memory_buffer b(0);

      WRITE_RANDOM_DATA(b, temp1, LEN);
      b.seek(OFF, Seek::SET);
      REQUIRE(b.position() == 0);
    }
    
    SECTION("seek past end should increase position but not capacity/size")
    {
      constexpr size_t LEN = 64, OFF = 128;
      memory_buffer b(0);
      
      WRITE_RANDOM_DATA(b, temp1, LEN);
      b.seek(OFF, Seek::END);
      REQUIRE(b.position() == LEN + OFF);
      REQUIRE(b.capacity() == LEN);
      REQUIRE(b.size() == LEN);
    }
    
    SECTION("seek past end should increase capacity/size on write and fill with 0s")
    {
      constexpr size_t LEN1 = 64, LEN2 = 32, OFF = 128;
      memory_buffer b(0);
      
      WRITE_RANDOM_DATA(b, temp1, LEN1);
      b.seek(OFF, Seek::END);
      WRITE_RANDOM_DATA(b, temp2, LEN2);
      
      REQUIRE(b.position() == LEN1 + OFF + LEN2);
      REQUIRE(b.capacity() == LEN1 + OFF + LEN2);
      REQUIRE(b.size() == LEN1 + OFF + LEN2);
      
      REQUIRE(std::equal(b.raw(), b.raw() + LEN1, temp1));
      REQUIRE(memcmp(b.raw() + LEN1 + OFF, temp2, LEN2) == 0);
      
      byte zero[OFF];
      memset(zero, 0, OFF);
      
      REQUIRE(std::equal(b.raw() + LEN1, b.raw() + LEN1 + OFF, zero));
    }
    
    SECTION("trim should reduce capacity accordingly")
    {
      constexpr size_t LEN = 64, CAP = 256;
      memory_buffer b(CAP);
      
      WRITE_RANDOM_DATA(b, temp1, LEN);
      REQUIRE(b.capacity() == CAP);
      REQUIRE(b.size() == LEN);
      REQUIRE(b.position() == LEN);
      b.trim();
      REQUIRE(b.capacity() == LEN);
      REQUIRE(b.size() == LEN);
      REQUIRE(b.position() == LEN);
    }
  }
  
  SECTION("write with references") {
    SECTION("simple write") {
      int c = 0x12345678, d;
      memory_buffer b(sizeof(int));
      data_reference<int> ref = b.reserve<int>();
      ref.write(c);
      
      REQUIRE(b.position() == sizeof(int));
      
      b.rewind();
      b.read(d);
      
      REQUIRE(b.position() == sizeof(int));
      REQUIRE(d == c);
    }
    
    SECTION("write after normal write") {
      constexpr size_t LEN = 64;
      int c = 0x12345678, d;
      memory_buffer b(sizeof(int));
      data_reference<int> ref = b.reserve<int>();
      
      WRITE_RANDOM_DATA(b, temp, LEN);
      
      ref.write(c);
      
      REQUIRE(b.position() == sizeof(int) + LEN);
      
      b.rewind();
      b.read(d);
      
      REQUIRE(b.position() == sizeof(int));
      REQUIRE(d == c);
    }

    SECTION("write/read array") {
      constexpr size_t LEN = 16;
      memory_buffer b(LEN*sizeof(int));
      array_reference<int> aref = b.reserveArray<int>(LEN);
      
      REQUIRE(b.capacity() == LEN*sizeof(int));
      
      for (int i = 0; i < LEN; ++i)
        aref.write(i*3, i);
      
      REQUIRE(b.size() == LEN*sizeof(int));
      REQUIRE(b.position() == LEN*sizeof(int));
      
      b.rewind();
      
      for (int i = 0; i < LEN; ++i)
      {
        int j;
        aref.read(j, i);
        REQUIRE(j == i*3);
      }
    }
    
    
  }
  
  SECTION("read") {
    SECTION("basic read")
    {
      constexpr size_t WLEN = 64, RLEN = 32;
      memory_buffer b(WLEN);
      
      WRITE_RANDOM_DATA(b, temp1, WLEN);
      b.rewind();
      READ_DATA(b, temp2, RLEN, read2);
      
      REQUIRE(b.position() == RLEN);
      REQUIRE(read2 == RLEN);
      REQUIRE(std::equal(temp1, temp1+RLEN, temp2));
    }
    
    SECTION("reading more than available should read less data")
    {
      constexpr size_t WLEN = 32, RLEN = 48;
      memory_buffer b(WLEN);
      
      WRITE_RANDOM_DATA(b, temp1, WLEN);
      b.rewind();
      READ_DATA(b, temp2, RLEN, read2);
      
      REQUIRE(b.position() == WLEN);
      REQUIRE(b.size() == WLEN);
      REQUIRE(read2 == WLEN);
      REQUIRE(std::equal(temp1, temp1+WLEN, temp2));
    }
  }
  
  SECTION("serialization") {
    constexpr size_t LEN = 128;
    memory_buffer b(LEN);
      
    WRITE_RANDOM_DATA(b, temp1, LEN);
    
    {
      file_handle out("test.bin", file_mode::WRITING);
      b.serialize(out);
    }
    
    {
      file_handle in("test.bin", file_mode::READING);
      REQUIRE(in.length() == LEN);
      
      memory_buffer v(LEN);
      v.unserialize(in);
      
      REQUIRE(b == v);
    }
    
  }
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

TEST_CASE("sha1", "[checksums]") {
  SECTION("sha1-test1") {
    std::string testString = "The quick brown fox jumps over the lazy dog";
    std::string sha1 = hash::sha1_digester::compute(testString.data(), testString.length());
    REQUIRE(sha1 == "2fd4e1c67a2d28fced849ee1bb76e7391b93eb12");
  }
  
  SECTION("md5-test2") {
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
}
