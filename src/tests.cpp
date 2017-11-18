#define CATCH_CONFIG_MAIN

#define CATCH_CONFIG_FAST_COMPILE

#include "libs/catch.h"

#include "core/memory_buffer.h"
#include "core/data_source.h"
#include "core/file_data_source.h"

#include "filters/filters.h"
#include "filters/deflate_filter.h"

#include "hash/hash.h"
#include "crypto/crypto.h"

#include "box/archive.h"



#include <random>

namespace support {
  u32 random(u32 modulo)
  {
    static std::random_device device;
    static std::default_random_engine engine(device());
    return engine() % modulo;
  }
}


TEST_CASE("optional", "[base]") {
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

TEST_CASE("biendian integers", "[base]") {
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

TEST_CASE("byte array / string conversions", "[base]")
{
  SECTION("test1") {
    byte data[] = { 0xf3, 0x44, 0x81, 0xec, 0x3c, 0xc6, 0x27, 0xba, 0xcd, 0x5d, 0xc3, 0xfb, 0x08, 0xf2, 0x73, 0xe6 };
    std::string converted = strings::fromByteArray(data, 16);
    std::vector<byte> reconverted = strings::toByteArray(converted);
    
    REQUIRE(converted == "f34481ec3cc627bacd5dc3fb08f273e6");
    REQUIRE(std::equal(reconverted.begin(), reconverted.end(), data));
  }
}

TEST_CASE("bit hacks", "[base]")
{
  SECTION("next power of two") {
    
    /* next power(x) when x is a power of two should be x */
    for (u64 i = 0; i < 64; ++i)
    {
      u64 x = 1ULL << i;
      REQUIRE(utils::nextPowerOfTwo(x) == x);
    }
    
    for (u64 i = 2; i < 64; ++i)
    {
      u64 x = 1ULL << i;
      u64 m = 0, M = (1LL << std::min(32ULL, i - 1)) - 1;
      
      for (size_t j = 0; j < 32; ++j)
      {
        u64 y = x - utils::random64(m, M);
        u64 z = utils::nextPowerOfTwo(y);
        REQUIRE(z == x);
      }
    }
    
    for (u64 i = 3; i < 64; ++i)
    {
      u64 x = 1ULL << i;
      u64 m = 0, M = (1LL << std::min(32ULL, i - 2)) - 1;
      
      for (size_t j = 0; j < 32; ++j)
      {
        u64 y = x - utils::random64(m, M) - (x >> 1);
        u64 z = utils::nextPowerOfTwo(y);
        REQUIRE(z == (x >> 1));
      }
    }
  }
}

#define WRITE_RANDOM_DATA_AND_REWIND(dest, name, length) byte name[(length)]; randomize(name, (length)); (dest).write(name, 1, (length)); (dest).rewind();
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
    
    SECTION("not owned data is not released on destruction")
    {
      constexpr size_t LEN = 256;
      
      byte* raw = new byte[LEN], *raw2 = new byte[LEN];
      for (size_t i = 0; i < LEN; ++i)
      {
        raw[i] = rand()%256;
        raw2[i] = raw[i];
      }
      
      {
        memory_buffer source(raw, 256, false);
      }
      
      REQUIRE(std::equal(raw, raw + LEN, raw2));
      raw[0] = !raw[0];
    }
    
    SECTION("move constructor takes ownership when data is owned")
    {
      constexpr size_t LEN = 256;
      memory_buffer source(LEN*2);
      
      WRITE_RANDOM_DATA(source, temp, LEN);
      
      const byte* raw = source.raw();
      
      memory_buffer dest(std::move(source));
      
      REQUIRE(dest.size() == LEN);
      REQUIRE(dest.position() == LEN);
      REQUIRE(dest.capacity() == LEN*2);
      REQUIRE(dest.raw() == raw);
      
      REQUIRE(source.size() == 0);
      REQUIRE(source.position() == 0);
      REQUIRE(source.capacity() == 0);
      REQUIRE(source.raw() == nullptr);
      
      REQUIRE(std::equal(dest.raw(), dest.raw() + LEN, temp));
    }
    
    SECTION("move constructor shares ownership when data is not owned")
    {
      constexpr size_t LEN = 256;
      
      byte* raw = new byte[LEN];
      for (size_t i = 0; i < LEN; ++i) raw[i] = rand()%256;
      
      memory_buffer source(raw, 256, false);
      
      memory_buffer dest(std::move(source));
      
      REQUIRE(dest.size() == LEN);
      REQUIRE(dest.position() == 0);
      REQUIRE(dest.capacity() == LEN);
      REQUIRE(dest.raw() == raw);
      
      REQUIRE(source.size() == dest.size());
      REQUIRE(source.position() == dest.position());
      REQUIRE(source.capacity() == dest.capacity());
      REQUIRE(source.raw() == dest.raw());
      
      raw[0] = !raw[0];
      
      REQUIRE(std::equal(dest.raw(), dest.raw() + LEN, raw));
      REQUIRE(std::equal(source.raw(), source.raw() + LEN, raw));

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
TEST_CASE("basic", "[stream]") {
  SECTION("single pipe") {
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
  
  SECTION("multiple fixed sinks") {
    constexpr size_t LEN = 1024;
    constexpr size_t L1 = 256LL, L2 = 256LL, L3 = 512LL;
    
    memory_buffer source;
    WRITE_RANDOM_DATA_AND_REWIND(source, test, LEN);

    sink_factory factory = []() { return new memory_buffer(); };
    multiple_fixed_size_sink_policy policy(factory, { L1, L2, L3 });
    multiple_data_sink sink(&policy);
    
    passthrough_pipe pipe(&source, &sink, 100);
    pipe.process();
    
    memory_buffer *sink1 = sink[0]->as<memory_buffer>(), *sink2 = sink[1]->as<memory_buffer>(), *sink3 = sink[2]->as<memory_buffer>();
    
    REQUIRE(sink1->size() == 256LL);
    REQUIRE(sink2->size() == 256LL);
    REQUIRE(sink3->size() == 512LL);
    REQUIRE(memcmp(source.raw(), sink1->raw(), L1) == 0);
    REQUIRE(memcmp(source.raw() + L1, sink2->raw(), L2) == 0);
    REQUIRE(memcmp(source.raw() + L1 + L2, sink3->raw(), L3) == 0);
  }
}

TEST_CASE("counter", "[filters]") {
  SECTION("data source counter") {
    constexpr size_t LEN = 2048;
    memory_buffer source;
    memory_buffer sink;
    unbuffered_source_filter<filters::data_counter> counter(&source);
    
    WRITE_RANDOM_DATA_AND_REWIND(source, test, LEN);
    
    passthrough_pipe pipe = passthrough_pipe(&counter, &sink, 8);
    pipe.process();

    REQUIRE(counter.filter().count() == LEN);
    REQUIRE(sink.size() == LEN);
  }
  
  SECTION("data sink counter") {
    constexpr size_t LEN = 2048;
    memory_buffer source;
    memory_buffer sink;
    unbuffered_sink_filter<filters::data_counter> counter(&sink);
    
    WRITE_RANDOM_DATA_AND_REWIND(source, test, LEN);
    
    passthrough_pipe pipe = passthrough_pipe(&source, &counter, 8);
    pipe.process();
    
    REQUIRE(counter.filter().count() == LEN);
    REQUIRE(sink.size() == LEN);
  }
  
  SECTION("data counter on both ends") {
    constexpr size_t LEN = 2048;
    memory_buffer source;
    memory_buffer sink;
    unbuffered_sink_filter<filters::data_counter> sinkCounter(&sink);
    unbuffered_source_filter<filters::data_counter> sourceCounter(&source);
    
    WRITE_RANDOM_DATA_AND_REWIND(source, test, LEN);
    
    passthrough_pipe pipe = passthrough_pipe(&sourceCounter, &sinkCounter, 8);
    pipe.process();
    
    REQUIRE(sinkCounter.filter().count() == LEN);
    REQUIRE(sourceCounter.filter().count() == LEN);
    REQUIRE(sink.size() == LEN);
  }
}
  
TEST_CASE("file sources/sinks", "[stream]") {
  SECTION("memory source to file sink / file source to memory sink") {
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
  
  SECTION("file source to file sink")
  {
    constexpr size_t LEN = 1024;
    memory_buffer source;
    
    WRITE_RANDOM_DATA_AND_REWIND(source, test, LEN);
    
    {
      file_data_sink fileSink("test.bin");
      passthrough_pipe pipe(&source, &fileSink, 64);
      pipe.process();
    }
    
    {
      file_data_source fileSource("test.bin");
      file_data_sink fileSink("test2.bin");
      passthrough_pipe pipe(&fileSource, &fileSink, 60);
      pipe.process();
    }
    
    {
      file_data_source fileSource("test2.bin");
      memory_buffer sink;
      passthrough_pipe pipe(&fileSource, &sink, 100);
      pipe.process();
      
      REQUIRE(source == sink);
    }
  }
}

TEST_CASE("hash filters", "[filters]") {
  SECTION("crc32") {
    constexpr size_t LEN = 256;
    memory_buffer source;
    memory_buffer sink;
    unbuffered_source_filter<filters::crc32_filter> filter(&source);
    hash::crc32_digester digester;
    
    WRITE_RANDOM_DATA_AND_REWIND(source, test, LEN);
    
    digester.update(test, LEN);
    hash::crc32_t value = digester.get();
    
    passthrough_pipe pipe(&filter, &sink, 30);
    pipe.process();
    
    REQUIRE(value == filter.filter().get());
  }
  
  SECTION("md5") {
    constexpr size_t LEN = 256;
    memory_buffer source;
    memory_buffer sink;
    unbuffered_source_filter<filters::md5_filter> filter(&source);
    hash::md5_digester digester;
    
    WRITE_RANDOM_DATA_AND_REWIND(source, test, LEN);
    
    digester.update(test, LEN);
    hash::md5_t value = digester.get();
    
    passthrough_pipe pipe(&filter, &sink, 30);
    pipe.process();
    
    REQUIRE(value == filter.filter().get());
  }
  
  SECTION("sha1") {
    constexpr size_t LEN = 256;
    memory_buffer source;
    memory_buffer sink;
    unbuffered_source_filter<filters::sha1_filter> filter(&source);
    hash::sha1_digester digester;
    
    WRITE_RANDOM_DATA_AND_REWIND(source, test, LEN);
    
    digester.update(test, LEN);
    hash::sha1_t value = digester.get();
    
    passthrough_pipe pipe(&filter, &sink, 30);
    pipe.process();
    
    REQUIRE(value == filter.filter().get());
  }
}

TEST_CASE("misc filters", "[filters]") {
  SECTION("xor filter") {
    constexpr size_t LEN = 256;
    const char* KEY = "foobar";
    constexpr size_t KEYLEN = 6;
    
    memory_buffer source;
    memory_buffer sink;
    memory_buffer sink2;
    
    WRITE_RANDOM_DATA_AND_REWIND(source, test, LEN);

    {
      source_filter<filters::xor_filter> filter(&source, 16, (const byte*)KEY, KEYLEN);
    
      passthrough_pipe pipe(&filter, &sink, 16);
      pipe.process();
    }
    
    {
      sink.rewind();
      source_filter<filters::xor_filter> filter(&sink, 16, (const byte*)KEY, KEYLEN);

      passthrough_pipe pipe(&filter, &sink2, 16);
      pipe.process();
    }

    REQUIRE(sink.size() == source.size());
    REQUIRE(sink2.size() == source.size());
    REQUIRE(source == sink2);
    
    size_t counter = 0;
    for (size_t i = 0; i < LEN; ++i)
    {
      test[i] ^= KEY[counter++];
      counter %= KEYLEN;
    }
    
    REQUIRE(std::equal(test, test + LEN, sink.raw()));
  }
  
  SECTION("skip filter on source") {
    constexpr size_t LEN = 256;
    size_t SKIP_AMOUNT = 0;
    size_t START_OFFSET = 0;
    size_t PASS = 0;
    size_t BUFFER_SIZE = 256;
    
    SECTION("buffer >= total, offset == 0, passthrough == 0") {
      SKIP_AMOUNT = 100;
    }
    
    SECTION("buffer >= total, offset != 0, passthrough == 0") {
      SKIP_AMOUNT = 100;
      START_OFFSET = 20;
    }
    
    SECTION("buffer >= total, offset 0, passthrough != 0") {
      SKIP_AMOUNT = 100;
      PASS = 60;
    }
    
    SECTION("buffer >= total, offset != 0, passthrough != 0") {
      START_OFFSET = 10;
      SKIP_AMOUNT = 41;
      PASS = 20;
    }
    
    memory_buffer source, sink;
    WRITE_RANDOM_DATA_AND_REWIND(source, test, LEN);
    
    source_filter<filters::skip_filter> filter(&source, BUFFER_SIZE, SKIP_AMOUNT, PASS, START_OFFSET);
    passthrough_pipe pipe(&filter, &sink, BUFFER_SIZE);
    pipe.process();
    
    size_t keptAfterSkip = PASS != 0 ? PASS : (LEN - SKIP_AMOUNT - START_OFFSET);
    
    REQUIRE(sink.size() == START_OFFSET + keptAfterSkip);
    REQUIRE(std::equal(source.raw(), source.raw() + START_OFFSET, sink.raw()));
    REQUIRE(std::equal(source.raw() + START_OFFSET + SKIP_AMOUNT, source.raw() + START_OFFSET + SKIP_AMOUNT + keptAfterSkip, sink.raw() + START_OFFSET));
  }
}

TEST_CASE("deflate", "[filters]") {
  SECTION("deflate/inflate source") {
    
    constexpr size_t LEN = 1 << 18;
    
    byte* testData = new byte[LEN];
    for (size_t i = 0; i < LEN; ++i)
      testData[i] = (i/8) % 256;
    
    memory_buffer source(testData, LEN);
    delete [] testData;
    
    source_filter<compression::deflater_filter> deflated(&source, 1024);
    
    memory_buffer sink;

    passthrough_pipe pipe(&deflated, &sink, 200);
    pipe.process();

    REQUIRE(deflated.filter().zstream().total_out == sink.size());
    
    memory_buffer source2(sink.raw(), sink.size());
    memory_buffer sink2;
    
    REQUIRE(source2.size() == sink.size());
    
    sink_filter<compression::inflater_filter> inflater(&sink2, 1024);
    
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
    
    source_filter<compression::deflater_filter> deflater(&source, 1024);
    source_filter<compression::inflater_filter> inflater(&deflater, 512);
    
    memory_buffer sink;
    
    passthrough_pipe pipe(&inflater, &sink, 1024);
    pipe.process();
    
    REQUIRE(deflater.filter().zstream().total_in == source.size());
    REQUIRE(deflater.filter().zstream().total_out == inflater.filter().zstream().total_in);
    REQUIRE(inflater.filter().zstream().total_out == source.size());
    
    REQUIRE(source == sink);
  }
  
  SECTION("chained deflate/inflate on sink")
  {
    constexpr size_t LEN = 1 << 18;
    
    byte* testData = new byte[LEN];
    for (size_t i = 0; i < LEN; ++i)
      testData[i] = (i/8) % 256;
    
    memory_buffer source(testData, LEN);
    delete [] testData;
    
    memory_buffer sink;
    sink_filter<compression::inflater_filter> inflater(&sink, 512);
    sink_filter<compression::deflater_filter> deflater(&inflater, 1024);
    
    passthrough_pipe pipe(&source, &deflater, 1024);
    pipe.process();
    
    REQUIRE(deflater.filter().zstream().total_in == source.size());
    REQUIRE(deflater.filter().zstream().total_out == inflater.filter().zstream().total_in);
    REQUIRE(inflater.filter().zstream().total_out == source.size());
    
    REQUIRE(source == sink);
  }
  
  SECTION("deflate on source / inflate on sink")
  {
    constexpr size_t LEN = 1 << 18;
    
    byte* testData = new byte[LEN];
    for (size_t i = 0; i < LEN; ++i)
      testData[i] = (i/8) % 256;
    
    memory_buffer source(testData, LEN);
    delete [] testData;
    memory_buffer sink;

    source_filter<compression::deflater_filter> deflater(&source, 500);
    sink_filter<compression::inflater_filter> inflater(&sink, 800);
    
    passthrough_pipe pipe(&deflater, &inflater, 1000);
    pipe.process();
    
    REQUIRE(deflater.filter().zstream().total_in == source.size());
    REQUIRE(deflater.filter().zstream().total_out == inflater.filter().zstream().total_in);
    REQUIRE(inflater.filter().zstream().total_out == source.size());
    
    REQUIRE(source == sink);
  }
}

#pragma mark hashes/crypto
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



TEST_CASE("aes", "[crypto]") {
  SECTION("aes128 ecb") {
    /* key, plain, cipher */
    const std::vector<std::tuple<std::string, std::string, std::string>> AES128_ECB_tests = {
      { "00000000000000000000000000000000", "f34481ec3cc627bacd5dc3fb08f273e6", "0336763e966d92595a567cc9ce537f5e" },
      { "00000000000000000000000000000000", "9798c4640bad75c7c3227db910174e72", "a9a1631bf4996954ebc093957b234589" },
      { "00000000000000000000000000000000", "96ab5c2ff612d9dfaae8c31f30c42168", "ff4f8391a6a40ca5b25d23bedd44a597" },
      { "00000000000000000000000000000000", "6a118a874519e64e9963798a503f1d35", "dc43be40be0e53712f7e2bf5ca707209" },
      { "00000000000000000000000000000000", "cb9fceec81286ca3e989bd979b0cb284", "92beedab1895a94faa69b632e5cc47ce" },
      { "00000000000000000000000000000000", "b26aeb1874e47ca8358ff22378f09144", "459264f4798f6a78bacb89c15ed3d601" },
      { "00000000000000000000000000000000", "58c8e00b2631686d54eab84b91f0aca1", "08a4e2efec8a8e3312ca7460b9040bbf" },
      { "80000000000000000000000000000000", "00000000000000000000000000000000", "0edd33d3c621e546455bd8ba1418bec8" },
      { "fffffffffffff0000000000000000000", "00000000000000000000000000000000", "7b90785125505fad59b13c186dd66ce3" },
      { "ffffffffffffffffffffffffff000000", "00000000000000000000000000000000", "2cb1dc3a9c72972e425ae2ef3eb597cd" }


    };
    
    for (const auto& test : AES128_ECB_tests)
    {
      crypto::AES128 aes;
      const std::vector<byte> key = strings::toByteArray(std::get<0>(test));
      const std::vector<byte> plain = strings::toByteArray(std::get<1>(test));

      std::vector<byte> cipher = std::vector<byte>(plain.size(), 0);
      aes.crypt(plain.data(), cipher.data(), key.data(), plain.size());
      
      REQUIRE(strings::fromByteArray(cipher) == std::get<2>(test));
      
      std::vector<byte> original = std::vector<byte>(plain.size(), 0);
      aes.decrypt(cipher.data(), original.data(), key.data(), plain.size());
      
      REQUIRE(plain == original);
    }
  }
  
  SECTION("aes192 ecb") {
    /* key, plain, cipher */
    const std::vector<std::tuple<std::string, std::string, std::string>> AES192_ECB_tests = {
      { "000000000000000000000000000000000000000000000000", "1b077a6af4b7f98229de786d7516b639", "275cfc0413d8ccb70513c3859b1d0f72" },
      { "000000000000000000000000000000000000000000000000", "9c2d8842e5f48f57648205d39a239af1", "c9b8135ff1b5adc413dfd053b21bd96d" },
      { "000000000000000000000000000000000000000000000000", "bff52510095f518ecca60af4205444bb", "4a3650c3371ce2eb35e389a171427440" },
      { "000000000000000000000000000000000000000000000000", "51719783d3185a535bd75adc65071ce1", "4f354592ff7c8847d2d0870ca9481b7c" },
      { "000000000000000000000000000000000000000000000000", "26aa49dcfe7629a8901a69a9914e6dfd", "d5e08bf9a182e857cf40b3a36ee248cc" },
      { "000000000000000000000000000000000000000000000000", "941a4773058224e1ef66d10e0a6ee782", "067cd9d3749207791841562507fa9626" },
      
    };
    
    for (const auto& test : AES192_ECB_tests)
    {
      crypto::AES192 aes;
      const std::vector<byte> key = strings::toByteArray(std::get<0>(test));
      const std::vector<byte> plain = strings::toByteArray(std::get<1>(test));
      
      std::vector<byte> cipher = std::vector<byte>(plain.size(), 0);
      aes.crypt(plain.data(), cipher.data(), key.data(), plain.size());
      
      REQUIRE(strings::fromByteArray(cipher) == std::get<2>(test));
      
      std::vector<byte> original = std::vector<byte>(plain.size(), 0);
      aes.decrypt(cipher.data(), original.data(), key.data(), plain.size());
      
      REQUIRE(plain == original);
    }
  }
  
  SECTION("aes256 ecb") {
    /* key, plain, cipher */
    const std::vector<std::tuple<std::string, std::string, std::string>> AES256_ECB_tests = {
      { "0000000000000000000000000000000000000000000000000000000000000000", "014730f80ac625fe84f026c60bfd547d", "5c9d844ed46f9885085e5d6a4f94c7d7" },
      { "0000000000000000000000000000000000000000000000000000000000000000", "0b24af36193ce4665f2825d7b4749c98", "a9ff75bd7cf6613d3731c77c3b6d0c04" },
      { "0000000000000000000000000000000000000000000000000000000000000000", "761c1fe41a18acf20d241650611d90f1", "623a52fcea5d443e48d9181ab32c7421" },
      { "0000000000000000000000000000000000000000000000000000000000000000", "8a560769d605868ad80d819bdba03771", "38f2c7ae10612415d27ca190d27da8b4" },
      { "0000000000000000000000000000000000000000000000000000000000000000", "91fbef2d15a97816060bee1feaa49afe", "1bc704f1bce135ceb810341b216d7abe" },
      
    };
    
    for (const auto& test : AES256_ECB_tests)
    {
      crypto::AES256 aes;
      const std::vector<byte> key = strings::toByteArray(std::get<0>(test));
      const std::vector<byte> plain = strings::toByteArray(std::get<1>(test));
      
      std::vector<byte> cipher = std::vector<byte>(plain.size(), 0);
      aes.crypt(plain.data(), cipher.data(), key.data(), plain.size());
      
      REQUIRE(strings::fromByteArray(cipher) == std::get<2>(test));
      
      std::vector<byte> original = std::vector<byte>(plain.size(), 0);
      aes.decrypt(cipher.data(), original.data(), key.data(), plain.size());
      
      REQUIRE(plain == original);
    }
  }
  
  SECTION("aes128 cbc") {
    /* key, iv, plain, cipher */
    const std::vector<std::tuple<std::string, std::string, std::string, std::string>> AES128_CBC_tests = {
      { "2b7e151628aed2a6abf7158809cf4f3c", "000102030405060708090A0B0C0D0E0F", "6bc1bee22e409f96e93d7e117393172a", "7649abac8119b246cee98e9b12e9197d" },
      { "2b7e151628aed2a6abf7158809cf4f3c", "7649ABAC8119B246CEE98E9B12E9197D", "ae2d8a571e03ac9c9eb76fac45af8e51", "5086cb9b507219ee95db113a917678b2" },
      { "2b7e151628aed2a6abf7158809cf4f3c", "5086CB9B507219EE95DB113A917678B2", "30c81c46a35ce411e5fbc1191a0a52ef", "73bed6b8e3c1743b7116e69e22229516" }

    };
    
    for (const auto& test : AES128_CBC_tests)
    {
      crypto::AES128 aes;
      const std::vector<byte> key = strings::toByteArray(std::get<0>(test));
      const std::vector<byte> iv = strings::toByteArray(std::get<1>(test));
      const std::vector<byte> plain = strings::toByteArray(std::get<2>(test));
      
      std::vector<byte> cipher = std::vector<byte>(plain.size(), 0);
      aes.cryptBuffer(plain.data(), cipher.data(), key.data(), plain.size(), iv.data());
      
      REQUIRE(strings::fromByteArray(cipher) == std::get<3>(test));
      
      std::vector<byte> original = std::vector<byte>(plain.size(), 0);
      aes.decryptBuffer(cipher.data(), original.data(), key.data(), plain.size(), iv.data());
      
      REQUIRE(plain == original);
    }
  }
  
  SECTION("aes192 cbc") {
    /* key, iv, plain, cipher */
    const std::vector<std::tuple<std::string, std::string, std::string, std::string>> AES192_CBC_tests = {
      { "8e73b0f7da0e6452c810f32b809079e562f8ead2522c6b7b", "4F021DB243BC633D7178183A9FA071E8", "ae2d8a571e03ac9c9eb76fac45af8e51", "b4d9ada9ad7dedf4e5e738763f69145a" },
      { "8e73b0f7da0e6452c810f32b809079e562f8ead2522c6b7b", "B4D9ADA9AD7DEDF4E5E738763F69145A", "30c81c46a35ce411e5fbc1191a0a52ef", "571b242012fb7ae07fa9baac3df102e0" },
      { "8e73b0f7da0e6452c810f32b809079e562f8ead2522c6b7b", "571B242012FB7AE07FA9BAAC3DF102E0", "f69f2445df4f9b17ad2b417be66c3710", "08b0e27988598881d920a9e64f5615cd" }
      
    };
    
    for (const auto& test : AES192_CBC_tests)
    {
      crypto::AES192 aes;
      const std::vector<byte> key = strings::toByteArray(std::get<0>(test));
      const std::vector<byte> iv = strings::toByteArray(std::get<1>(test));
      const std::vector<byte> plain = strings::toByteArray(std::get<2>(test));
      
      std::vector<byte> cipher = std::vector<byte>(plain.size(), 0);
      aes.cryptBuffer(plain.data(), cipher.data(), key.data(), plain.size(), iv.data());
      
      REQUIRE(strings::fromByteArray(cipher) == std::get<3>(test));
      
      std::vector<byte> original = std::vector<byte>(plain.size(), 0);
      aes.decryptBuffer(cipher.data(), original.data(), key.data(), plain.size(), iv.data());
      
      REQUIRE(plain == original);
    }
  }
  
  SECTION("aes256 cbc") {
    /* key, iv, plain, cipher */
    const std::vector<std::tuple<std::string, std::string, std::string, std::string>> AES256_CBC_tests = {
      { "603deb1015ca71be2b73aef0857d77811f352c073b6108d72d9810a30914dff4", "000102030405060708090A0B0C0D0E0F", "6bc1bee22e409f96e93d7e117393172a", "f58c4c04d6e5f1ba779eabfb5f7bfbd6" },
      { "603deb1015ca71be2b73aef0857d77811f352c073b6108d72d9810a30914dff4", "F58C4C04D6E5F1BA779EABFB5F7BFBD6", "ae2d8a571e03ac9c9eb76fac45af8e51", "9cfc4e967edb808d679f777bc6702c7d" },
      { "603deb1015ca71be2b73aef0857d77811f352c073b6108d72d9810a30914dff4", "9CFC4E967EDB808D679F777BC6702C7D", "30c81c46a35ce411e5fbc1191a0a52ef", "39f23369a9d9bacfa530e26304231461" }
      
    };
    
    for (const auto& test : AES256_CBC_tests)
    {
      crypto::AES256 aes;
      const std::vector<byte> key = strings::toByteArray(std::get<0>(test));
      const std::vector<byte> iv = strings::toByteArray(std::get<1>(test));
      const std::vector<byte> plain = strings::toByteArray(std::get<2>(test));
      
      std::vector<byte> cipher = std::vector<byte>(plain.size(), 0);
      aes.cryptBuffer(plain.data(), cipher.data(), key.data(), plain.size(), iv.data());
      
      REQUIRE(strings::fromByteArray(cipher) == std::get<3>(test));
      
      std::vector<byte> original = std::vector<byte>(plain.size(), 0);
      aes.decryptBuffer(cipher.data(), original.data(), key.data(), plain.size(), iv.data());
      
      REQUIRE(plain == original);
    }
  }
}

#pragma mark Filter Queue

TEST_CASE("filter builder queue", "[box archive]") {
  constexpr size_t LEN = 256;
  const char *key1 = "foobar", *key2 = "baz";
  
  memory_buffer source(LEN);
  memory_buffer sink(LEN);
  WRITE_RANDOM_DATA_AND_REWIND(source, temp, LEN);
  
  
  
  filter_builder_queue queue;
  
  queue.add(new builders::xor_builder(16, key1));
  queue.add(new builders::xor_builder(16, key2));
  
  filter_cache cache = queue.apply(&source);
  data_source* result = cache.get();
  
  passthrough_pipe pipe(result, &sink, 16);
  pipe.process();
  
  for (size_t i = 0; i < LEN; ++i)
    temp[i] = temp[i] ^ key1[i % 6] ^ key2[i % 3];
  
  REQUIRE(sink.size() == source.size());
  REQUIRE(std::equal(sink.raw(), sink.raw() + LEN, temp));
  
  filter_cache rcache = queue.unapply(&sink);
  data_source* rsource = rcache.get();
  memory_buffer original(LEN);
  
  {
    sink.rewind();
    passthrough_pipe pipe(rsource, &original, 16);
    pipe.process();
  }
  
  REQUIRE(original.size() == source.size());
  REQUIRE(original == source);
}

TEST_CASE("payload generation", "[box archive]")
{
  const std::string key = "foobar";
  const std::string key2 = "baz";

  
  SECTION("single xor filter") {
    const auto builder = builders::xor_builder(16, key);
    const auto payload = builder.payload();
    
    REQUIRE(payload.size() == sizeof(box::slength_t) + key.length());
  }
  
  SECTION("builder queue with single filter") {
    filter_builder_queue queue;
    queue.add(new builders::xor_builder(16, key));
    memory_buffer payload = queue.payload();
    
    REQUIRE(payload.size() == sizeof(box::Payload) + sizeof(box::slength_t) + key.length());
  }
  
  SECTION("builder queue with double filter") {
    filter_builder_queue queue;
    queue.add(new builders::xor_builder(16, key));
    queue.add(new builders::xor_builder(16, key2));

    memory_buffer payload = queue.payload();
    
    size_t finalPayloadLength = sizeof(box::Payload)*2 + sizeof(box::slength_t)*2 + key.length() + key2.length();
    
    REQUIRE(payload.size() == finalPayloadLength);
    
    const box::Payload* first = (const box::Payload*)payload.raw();
    
    REQUIRE(first->identifier == builders::identifier::XOR_FILTER);
    REQUIRE(first->length == sizeof(box::Payload) + sizeof(box::slength_t) + key.length());
    REQUIRE(first->hasNext == true);
    
    const box::Payload* second = (const box::Payload*)(payload.raw() + first->length);
    
    REQUIRE(second->identifier == builders::identifier::XOR_FILTER);
    REQUIRE(second->length == sizeof(box::Payload) + sizeof(box::slength_t) + key2.length());
    REQUIRE(second->hasNext == false);
  }
}

#pragma mark Box Archive

TEST_CASE("empty archive", "[box archive]") {
  memory_buffer buffer;
  
  Archive source, result;
  
  source.options().checksum.calculateGlobalChecksum = true;
  source.options().checksum.digesterBuffer = KB16;
  
  source.write(buffer);
  result.read(buffer);
  
  REQUIRE(buffer.size() == sizeof(box::Header));
  REQUIRE(buffer.size() == result.header().fileLength);
  
  REQUIRE(result.header().entryTableOffset == sizeof(box::Header));
  REQUIRE(result.header().streamTableOffset == sizeof(box::Header));
  REQUIRE(result.header().nameTableOffset == sizeof(box::Header));
  
  REQUIRE(result.isValidMagicNumber());
  REQUIRE(result.header().hasFlag(box::HeaderFlag::INTEGRITY_CHECKSUM_ENABLED));
  REQUIRE(result.isValidGlobalChecksum(buffer));
}

struct ArchiveTestEntry
{
  std::string name;
  seekable_data_source* source;
  std::vector<filter_builder*> builders;
};

TEST_CASE("simple archive (one entry per stream)", "[box archive]") {
  constexpr size_t MIN_LEN = 128, MAX_LEN = 256, LEN = 256;
  std::vector<std::tuple<std::string, seekable_data_source*>> entries;
  
  SECTION("single entry") {
    const size_t length = utils::random64(MIN_LEN, MAX_LEN);
    memory_buffer* source = new memory_buffer(length);
    WRITE_RANDOM_DATA_AND_REWIND(*source, temp, length);
    entries.push_back({ "foobar.bin", source });
  }
  
  SECTION("two entries") {
    for (size_t i = 0; i < 2; ++i)
    {
      const size_t length = utils::random64(MIN_LEN, MAX_LEN);
      memory_buffer* source = new memory_buffer(length);
      WRITE_RANDOM_DATA_AND_REWIND(*source, temp, length);
      entries.push_back({ fmt::sprintf("entry%lu.bin", i), source });
    }
  }
  
  SECTION("ten entries") {
    for (size_t i = 0; i < 10; ++i)
    {
      const size_t length = utils::random64(MIN_LEN, MAX_LEN);
      memory_buffer* source = new memory_buffer(length);
      WRITE_RANDOM_DATA_AND_REWIND(*source, temp, length);
      entries.push_back({ fmt::sprintf("entry%lu.bin", i), source });
    }
  }
  
  memory_buffer destination;
  
  Archive archive = Archive::ofOneEntryPerStream(entries, { });
  archive.write(destination);
  
  REQUIRE(archive.entries().size() == entries.size());
  REQUIRE(archive.streams().size() == entries.size());
  
  /* expected size */
  const size_t archiveSize =
    sizeof(box::Header)
  + sizeof(box::Entry) * entries.size()
  + sizeof(box::Stream) * entries.size()
  + std::accumulate(entries.begin(), entries.end(), 0UL, [] (size_t count, const decltype(entries)::value_type& tuple) { return std::get<0>(tuple).size() + 1 + count; })
  + std::accumulate(entries.begin(), entries.end(), 0UL, [] (size_t count, const decltype(entries)::value_type& tuple) { return std::get<1>(tuple)->size() + count; })
  ;
  
  REQUIRE(destination.size() == archiveSize);
  
  Archive verify;
  verify.read(destination);

  REQUIRE(verify.entries().size() == entries.size());
  REQUIRE(verify.streams().size() == entries.size());
  
  for (size_t i = 0; i < verify.entries().size(); ++i)
  {
    const ArchiveEntry& archiveEntry = archive.entries()[i];
    const auto& entry = archiveEntry.binary();
    
    REQUIRE(archiveEntry.filters().size() == 0);
    
    size_t length = std::get<1>(entries[i])->size();
    REQUIRE(entry.compressedSize == length);
    REQUIRE(entry.originalSize == length);
    REQUIRE(entry.filteredSize == length);
    REQUIRE(entry.indexInStream == 0);
    REQUIRE(entry.stream == i);
    
    REQUIRE(archiveEntry.name() == std::get<0>(entries[i])); // name match
    
    memory_buffer sverify;
    ArchiveReadHandle handle(destination, verify, archiveEntry);
    
    passthrough_pipe pipe(handle.source(true), &sverify, entry.originalSize);
    pipe.process();
        
    REQUIRE(sverify == *(memory_buffer*)std::get<1>(entries[i]));
  }
  
  for (size_t i = 0; i < verify.streams().size(); ++i)
  {
    const ArchiveStream& streamEntry = archive.streams()[i];
    const auto& stream = streamEntry.binary();
    
    size_t length = std::get<1>(entries[i])->size();
    REQUIRE(stream.length == length);
    REQUIRE(streamEntry.entries().size() == 1);
    REQUIRE(streamEntry.entries()[0] == i);
  }
  
  std::for_each(entries.begin(), entries.end(), [](const decltype(entries)::value_type& tuple) { delete std::get<1>(tuple); });
}

TEST_CASE("single entry archive", "[box archive]") {
  size_t LEN = 0;
  memory_buffer source, destination;
  std::initializer_list<filter_builder*> filters;

  SECTION("no filters") {
    LEN = 256;
    filters = {};
    WRITE_RANDOM_DATA_AND_REWIND(source, temp, LEN);
  }
  
  SECTION("xor filter") {
    LEN = 1024;
    filters = { new builders::xor_builder(LEN, "foobar") };
    WRITE_RANDOM_DATA_AND_REWIND(source, temp, LEN);
  }
  
  SECTION("zlib deflate filter") {
    LEN = KB16;
    filters = { new builders::deflate_builder(LEN) };
    source.reserve(KB16);
    for (size_t i = 0; i < LEN; ++i)
      source.raw()[i] = i / (KB16 / 256);
    source.rewind();
  }
  
  Archive archive = Archive::ofSingleEntry("foobar.bin", &source, filters);
  archive.write(destination);
  
  REQUIRE(archive.entries().size() == 1);
  REQUIRE(archive.streams().size() == 1);
  
  /* expected size */
  size_t destinationSize =
  sizeof(box::Header) /* header */
  + sizeof(box::Entry)*1 + sizeof(box::Stream)*1 /* stream and entry tables */
  + strlen("foobar.bin") + 1 /* entry file name */
  + archive.entries()[0].binary().compressedSize /* stream */
  + archive.entries()[0].binary().payloadLength
  ;
  
  destination.rewind();
  
  REQUIRE(destination.size() == destinationSize);

  const ArchiveEntry& archiveEntry = archive.entries()[0];
  const auto& entry = archiveEntry.binary();

  REQUIRE(archiveEntry.filters().size() == filters.size());
  
  //REQUIRE(entry.compressedSize == LEN);
  REQUIRE(entry.originalSize == LEN);
  //REQUIRE(entry.filteredSize == LEN);
  REQUIRE(entry.indexInStream == 0);
  REQUIRE(entry.stream == 0);
  
  const ArchiveStream& streamEntry = archive.streams()[0];
  const auto& stream = streamEntry.binary();
  
  REQUIRE(stream.length == entry.compressedSize);
  REQUIRE(streamEntry.entries().size() == 1);
  REQUIRE(streamEntry.entries()[0] == 0);
  
  Archive verify;
  verify.read(destination);
  
  REQUIRE(archive.entries().size() == verify.entries().size());
  REQUIRE(archive.streams().size() == verify.streams().size());
  
  memory_buffer sverify;
  ArchiveReadHandle handle(destination, verify, archiveEntry);
  
  passthrough_pipe pipe(handle.source(true), &sverify, entry.originalSize);
  pipe.process();
  
  REQUIRE(sverify == source);

}
