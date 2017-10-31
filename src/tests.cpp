#define CATCH_CONFIG_MAIN
#include "libs/catch.h"

#include "core/memory_buffer.h"
#include "core/data_source.h"
#include "core/filters.h"
#include "core/file_data_source.h"
#include "zip/zip_mutator.h"
#include "hash/hash.h"



#include <random>

namespace support {
  u32 random(u32 modulo)
  {
    static std::random_device device;
    static std::default_random_engine engine(device());
    return engine() % modulo;
  }
}


TEST_CASE("catch library correctly setup", "[setup]") {
  REQUIRE(true);
}

TEST_CASE("optional", "[optional values]") {
  SECTION("u32") {
    constexpr u32 VALUE = 0x12345678;
    
    optional<u32> value;
    REQUIRE(!value.isPresent());
    value.set(VALUE);
    REQUIRE(value.isPresent());
    REQUIRE(value.get() == VALUE);
    value.clear();
    REQUIRE(!value.isPresent());

  }
}

TEST_CASE("biendian integers", "[endianness]") {
  SECTION("u32 same endianness") {
    constexpr u32 VALUE = 0x12345678;

    u32 x = VALUE;
    u32se se = VALUE;
    
    REQUIRE(x == se.operator u32());
    REQUIRE(memcmp(&x, &se, sizeof(u32)) == 0);
  }
  
  SECTION("u32 reversed endianness") {
    u32 x = 0x12345678;
    u32de de = 0x12345678;
    
    const byte* p1 = reinterpret_cast<const byte*>(&x);
    const byte* p2 = reinterpret_cast<const byte*>(&de);
    
    REQUIRE(p1[0] == p2[3]);
    REQUIRE(p1[1] == p2[2]);
    REQUIRE(p1[2] == p2[1]);
    REQUIRE(p1[3] == p2[0]);
  }
  
  SECTION("u32 same endianness from memory")
  {
    byte buffer[] = { 0x12, 0x34, 0x56, 0x78 };
    u32se x;
    
    memcpy(&x, buffer, sizeof(u32se));
    
    u32 y = x;
    
    REQUIRE(memcmp(buffer, &y, sizeof(u32)) == 0);
  }
  
  SECTION("u32 reversed endianness from memory")
  {
    byte buffer[] = { 0x12, 0x34, 0x56, 0x78 };
    u32de x;
    
    memcpy(&x, buffer, sizeof(u32de));
    
    u32 y = x;
    
    const byte* p = reinterpret_cast<const byte*>(&y);
    REQUIRE(buffer[0] == p[3]);
    REQUIRE(buffer[1] == p[2]);
    REQUIRE(buffer[2] == p[1]);
    REQUIRE(buffer[3] == p[0]);
  }
  
  SECTION("u16 same endianness") {
    constexpr u16 VALUE = 0x1234;

    u16 x = VALUE;
    u16se se = VALUE;
    
    REQUIRE(x == se.operator u16());
    REQUIRE(memcmp(&x, &se, sizeof(u16)) == 0);
  }
  
  SECTION("u16 reversed endianness") {
    constexpr u16 VALUE = 0x1234;
    
    u16 x = VALUE;
    u16de de = VALUE;
    
    const byte* p1 = reinterpret_cast<const byte*>(&x);
    const byte* p2 = reinterpret_cast<const byte*>(&de);
    
    REQUIRE(p1[1] == p2[0]);
    REQUIRE(p1[0] == p2[1]);
  }

}

#define WRITE_RANDOM_DATA_AND_REWIND(dest, name, length) byte name[(length)]; randomize(name, (length)); dest.write(name, 1, (length)); dest.rewind();
#define WRITE_RANDOM_DATA(dest, name, length) byte name[(length)]; randomize(name, (length)); dest.write(name, 1, (length));
void randomize(byte* data, size_t len) { for (size_t i = 0; i < len; ++i) { data[i] = support::random(256); } }
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
      b.seek(0, Seek::END);
      
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
      
      WRITE_RANDOM_DATA_AND_REWIND(b, temp1, WLEN);
      READ_DATA(b, temp2, RLEN, read2);
      
      REQUIRE(b.position() == RLEN);
      REQUIRE(read2 == RLEN);
      REQUIRE(std::equal(temp1, temp1+RLEN, temp2));
    }
    
    SECTION("reading more than available should read less data")
    {
      constexpr size_t WLEN = 32, RLEN = 48;
      memory_buffer b(WLEN);
      
      WRITE_RANDOM_DATA_AND_REWIND(b, temp1, WLEN);
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

#pragma mark streams
TEST_CASE("streams", "[stream]") {
  SECTION("single pipe") {
    constexpr size_t LEN = 256;
    memory_buffer source;
    memory_buffer sink;
    
    WRITE_RANDOM_DATA_AND_REWIND(source, test, LEN);
    
    passthrough_pipe pipe = passthrough_pipe(&source, &sink, 8);
    pipe.process();
    
    REQUIRE(source == sink);
  }
  
  SECTION("basic pipe test") {
    constexpr size_t LEN = 256;
    memory_buffer source;
    memory_buffer sink;
    
    WRITE_RANDOM_DATA_AND_REWIND(source, test, LEN);
    
    passthrough_pipe pipe = passthrough_pipe(&source, &sink, 8);
    pipe.process();
    
    REQUIRE(source == sink);
  }
  
  SECTION("multiple sources") {
    constexpr size_t LEN1 = 1024, LEN2 = 512;
    memory_buffer source1, source2;
    memory_buffer sink;
    multiple_data_source multiple_source({&source1, &source2});
    
    WRITE_RANDOM_DATA_AND_REWIND(source1, test1, LEN1);
    WRITE_RANDOM_DATA_AND_REWIND(source2, test2, LEN2);

    passthrough_pipe pipe(&multiple_source, &sink, 30);
    pipe.process();
    
    REQUIRE(sink.size() == LEN1 + LEN2);
    REQUIRE(std::equal(sink.raw(), sink.raw() + LEN1, source1.raw()));
    REQUIRE(std::equal(sink.raw() + LEN1, sink.raw() + LEN1 + LEN2, source2.raw()));
  }
  
  SECTION("data read counter") {
    constexpr size_t LEN = 2048;
    memory_buffer source;
    memory_buffer sink;
    source_filter<filters::data_counter> counter(&source);
    
    WRITE_RANDOM_DATA_AND_REWIND(source, test, LEN);
    
    passthrough_pipe pipe = passthrough_pipe(&counter, &sink, 8);
    pipe.process();

    REQUIRE(counter.count() == LEN);
    REQUIRE(sink.size() == LEN);
  }
  
  SECTION("data write counter") {
    constexpr size_t LEN = 2048;
    memory_buffer source;
    memory_buffer sink;
    sink_filter<filters::data_counter> counter(&sink);
    
    WRITE_RANDOM_DATA_AND_REWIND(source, test, LEN);
    
    passthrough_pipe pipe = passthrough_pipe(&source, &counter, 8);
    pipe.process();
    
    REQUIRE(counter.count() == LEN);
    REQUIRE(sink.size() == LEN);
  }
  
  SECTION("data counter on both ends") {
    constexpr size_t LEN = 2048;
    memory_buffer source;
    memory_buffer sink;
    sink_filter<filters::data_counter> sinkCounter(&sink);
    source_filter<filters::data_counter> sourceCounter(&source);
    
    WRITE_RANDOM_DATA_AND_REWIND(source, test, LEN);
    
    passthrough_pipe pipe = passthrough_pipe(&sourceCounter, &sinkCounter, 8);
    pipe.process();
    
    REQUIRE(sinkCounter.count() == LEN);
    REQUIRE(sourceCounter.count() == LEN);
    REQUIRE(sink.size() == LEN);
  }
  
  SECTION("file data source/sink") {
    constexpr size_t LEN = 1024;
    memory_buffer source, sink;
    
    WRITE_RANDOM_DATA_AND_REWIND(source, test, LEN);
    
    {
      file_data_sink fileSink("test.bin");
      passthrough_pipe pipe(&source, &fileSink, 64);
      pipe.process();
    }
    
    {
      file_data_source fileSource("test.bin");
      passthrough_pipe pipe(&fileSource, &sink, 64);
      pipe.process();
    }
    
    REQUIRE(source == sink);
  }
  
  SECTION("filters") {
    SECTION("crc32") {
      constexpr size_t LEN = 256;
      memory_buffer source;
      memory_buffer sink;
      source_filter<filters::crc32_filter> filter(&source);
      hash::crc32_digester digester;
      
      WRITE_RANDOM_DATA_AND_REWIND(source, test, LEN);
      
      digester.update(test, LEN);
      hash::crc32_t value = digester.get();
      
      passthrough_pipe pipe(&filter, &sink, 30);
      pipe.process();
      
      REQUIRE(value == filter.get());
    }
    
    SECTION("md5") {
      constexpr size_t LEN = 256;
      memory_buffer source;
      memory_buffer sink;
      source_filter<filters::md5_filter> filter(&source);
      hash::md5_digester digester;
      
      WRITE_RANDOM_DATA_AND_REWIND(source, test, LEN);
      
      digester.update(test, LEN);
      hash::md5_t value = digester.get();
      
      passthrough_pipe pipe(&filter, &sink, 30);
      pipe.process();
      
      REQUIRE(value == filter.get());
    }
    
    SECTION("sha1") {
      constexpr size_t LEN = 256;
      memory_buffer source;
      memory_buffer sink;
      source_filter<filters::sha1_filter> filter(&source);
      hash::sha1_digester digester;
      
      WRITE_RANDOM_DATA_AND_REWIND(source, test, LEN);
      
      digester.update(test, LEN);
      hash::sha1_t value = digester.get();
      
      passthrough_pipe pipe(&filter, &sink, 30);
      pipe.process();
      
      REQUIRE(value == filter.get());
    }
  }
  
  SECTION("compression") {
    /*SECTION("deflate/inflate source") {
      
      constexpr size_t LEN = 1 << 18;
      
      byte* testData = new byte[LEN];
      for (size_t i = 0; i < LEN; ++i)
        testData[i] = (i/8) % 256;
      
      memory_buffer source(testData, LEN);
      delete [] testData;
      
      buffered_source_filter<compression::deflater_filter> deflated(&source, 1024);
      
      memory_buffer sink;

      passthrough_pipe pipe(&deflated, &sink, 200);
      pipe.process();

      REQUIRE(deflated.zstream().total_out == sink.size());
      
      memory_buffer source2(sink.raw(), sink.size());
      memory_buffer sink2;
      
      REQUIRE(source2.size() == sink.size());
      
      buffered_sink_filter<compression::inflater_filter> inflater(&sink2, 1024);
      
      passthrough_pipe pipe2(&source2, &inflater, 200);

      pipe2.process();
      
      REQUIRE(sink2 == source);
    }
    
    SECTION("chained deflate/inflate on source")
    {
      constexpr size_t LEN = 1 << 18;
      
      byte* testData = new byte[LEN];
      for (size_t i = 0; i < LEN; ++i)
        testData[i] = (i/8) % 256;
      
      memory_buffer source(testData, LEN);
      delete [] testData;
      
      buffered_source_filter<compression::deflater_filter> deflater(&source, 1024);
      buffered_source_filter<compression::inflater_filter> inflater(&deflater, 512);
      
      memory_buffer sink;
      
      passthrough_pipe pipe(&inflater, &sink, 1024);
      pipe.process();
      
      REQUIRE(deflater.zstream().total_in == source.size());
      REQUIRE(deflater.zstream().total_out == inflater.zstream().total_in);
      REQUIRE(inflater.zstream().total_out == source.size());
      
      REQUIRE(source == sink);
    }*/
    
    SECTION("chained deflate/inflate on sink")
    {
      constexpr size_t LEN = 1 << 18;
      
      byte* testData = new byte[LEN];
      for (size_t i = 0; i < LEN; ++i)
        testData[i] = (i/8) % 256;
      
      memory_buffer source(testData, LEN);
      delete [] testData;
      
      memory_buffer sink;
      buffered_sink_filter<compression::inflater_filter> inflater(&sink, 512);
      buffered_sink_filter<compression::deflater_filter> deflater(&inflater, 1024);
      
      passthrough_pipe pipe(&source, &deflater, 1024);
      pipe.process();
      
      REQUIRE(deflater.zstream().total_in == source.size());
      REQUIRE(deflater.zstream().total_out == inflater.zstream().total_in);
      REQUIRE(inflater.zstream().total_out == source.size());
      
      REQUIRE(source == sink);
    }
  }
}

#pragma mark hashes
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
  
  SECTION("sha1-test2") {
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
  
  SECTION("1 byte length") {
    byte data[1] = { 'a' };
    std::string sha1 = hash::sha1_digester::compute(data, 1);
    REQUIRE(sha1 == "86f7e437faa5a7fce15d1ddcb9eaeaea377667b8");
  }
  
  SECTION("1 block length") {
    std::string testString = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    REQUIRE(testString.length() == 64);
    std::string sha1 = hash::sha1_digester::compute(testString.data(), testString.length());
    REQUIRE(sha1 == "ce4303f6b22257d9c9cf314ef1dee4707c6e1c13");
  }
  
  SECTION("1 block less 1 byte length") {
    std::string testString = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcde";
    REQUIRE(testString.length() == 63);
    std::string sha1 = hash::sha1_digester::compute(testString.data(), testString.length());
    REQUIRE(sha1 == "ef717286343f6da3f4e6f68c6de02a5148a801c4");
  }
  
  SECTION("partial unaligned updates")
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
      size_t current = support::random((u32)std::min(available+1, 64UL));
      digester.update(testString.data() + (length - available), current);
      available -= current;
    }
    
    std::string sha1 = digester.get();
    REQUIRE(sha1 == "a851751e1e14c39a78f0a4b8debf69dba0b2ae0d");
  }
}
