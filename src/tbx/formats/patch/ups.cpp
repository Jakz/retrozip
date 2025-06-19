#include "ips.h"

#include "tbx/streams/data_source.h"
#include "tbx/streams/memory_buffer.h"
#include "tbx/streams/file_data_source.h"
#include "tbx/streams/data_filter.h"
#include "tbx/streams/vector_stream.h"
#include "filters/filters.h"

using namespace patch::ups;

uint64_t Patch::readVariableInt(data_source* src)
{
  uint64_t result = 0;
  uint64_t shift = 1;

  while (true)
  {
    uint8_t byte;
    src->read(byte);
    result += (byte & 0x7F) * shift;
    if (byte & 0x80) /* stop bit */
      break;

    shift <<= 7;
    result += shift;
  }

  return result;
}

void Patch::writeVariableInt(data_sink* sink, uint64_t value)
{
  while (true)
  {
    uint64_t current = value & 0x7F;
    bool last = ((value >> 7) == 0);
    
    value >>= 7;

    if (last)
    {
      uint8_t byte = current | 0x80;
      sink->write(&byte, 1);
      break;
    }
    else
    {
      value -= 1;
      
      uint8_t byte = current;
      sink->write(&byte, 1);
    }
  }
}

Status Patch::load(seekable_data_source* source)
{
  static_assert(sizeof(Checksum) == sizeof(uint32_t) * 3);

  auto digester = unbuffered_source_filter<filters::crc32_filter>(source);
  
  /* read header */
  digester.read((byte*)&_header.magic[0], 4);
  _header.inputSize = readVariableInt(&digester);
  _header.outputSize = readVariableInt(&digester);
  _header.headerSizeInBytes = source->tell();

  /* read data */
  size_t amount = source->size() - source->tell() - sizeof(Checksum);
  _data.resize(amount);
  digester.read(_data.data(), amount);

  /* read checksums */
  //TODO: this is assuming platform is little-endian
  /* read two checksums only because last one must be compared against whole data up to here */
  digester.read((byte*)&_checksum.inputChecksum, sizeof(uint32_t));
  digester.read((byte*)&_checksum.outputChecksum, sizeof(uint32_t));
  uint32_t checksum = digester.filter().get();
  digester.read((byte*)&_checksum.patchChecksum, sizeof(uint32_t));

  if (checksum != _checksum.patchChecksum)
    return Status::InvalidPatchChecksum;

  return Status::Ok;
}

Status Patch::apply(seekable_data_source* source, data_sink* sink) const
{
  bool verifySource = true;
  bool verifyOutput = true;
  
  bool finished = false;
  const uint8_t* data = _data.data();
  const uint8_t* dataStart = data;
  const uint8_t* dataEnd = data + _data.size();

  weak_data_source weakData = weak_data_source(_data);

  size_t sourceOffset = 0;
  size_t written = 0;

  memory_buffer buffer;

  if (source->size() != _header.inputSize)
    return Status::InvalidSourceSize;

  while (data < dataEnd)
  {
    /* read offset */
    auto* dataBefore = data;
    size_t amountToCopy = readVariableInt(&weakData);
    size_t offset = 0;
    //offset += sourceOffset;

    /* copy bytes until offset is correct */
    //size_t amountToCopy = offset - sourceOffset;
    TRACE("%p: Patch::apply: %08x  sourceOffset = %08x -> %08x, amountToCopy = %lu", this, _header.headerSizeInBytes + dataBefore - dataStart, sourceOffset, sourceOffset + amountToCopy, amountToCopy);

    buffer.ensure_capacity(amountToCopy + 1);
    buffer.seek(0);
    memset(buffer.direct(), 0, amountToCopy);

    auto actual = source->read(buffer.direct(), amountToCopy);

    sink->write(buffer.direct(), amountToCopy);

    written += amountToCopy;
    sourceOffset += amountToCopy;

    assert(((file_data_sink*)sink)->tell() == written);

    size_t before = written;

    /* now keep xoring until a byte it's equal both on source and patch */
    do
    {
      dataBefore = data;
      uint8_t patchByte = *data++;
      uint8_t sourceByte = 0;

      if (sourceOffset < source->size())
        source->read(sourceByte);

      uint8_t result = patchByte ^ sourceByte;
      //TRACE("%p: Patch::apply %02x ^ %02x = %02x, sourceOffset = %08x", this, patchByte, sourceByte, result, sourceOffset);

      sink->write(&result, 1);
      ++written;
      ++sourceOffset;

      if (patchByte == 0)
      {
        TRACE("%p: Patch::apply: %08x written = %lu, sourceOffset = %08x", this, _header.headerSizeInBytes + dataBefore - dataStart, written - before, sourceOffset);
        break;
      }

      if (data >= dataEnd)
        break;

    } while (true);
  }

  /* if patch didn't produce enough bytes just fill with zeros */
  if (written < _header.outputSize)
  {
    size_t remaining = _header.outputSize - written;
    buffer.ensure_capacity(remaining);
    buffer.seek(0);
    memset(buffer.direct(), 0, remaining);
    sink->write(buffer.direct(), remaining);
    written += remaining;
  }

  return Status::Ok;
}

Status Patch::generate(seekable_data_source* source, seekable_data_source* patched)
{  
  size_t relative = 0;
  size_t streak = 0;

  auto wrapped = weak_vector_sink(_data);
  
  bool finished = false;

  while (!finished)
  {
    uint8_t sbyte, pbyte;
    
    auto ss = source->read(&sbyte, 1);
    auto ps = patched->read(&pbyte, 1);

    if (ss == ps)
      ++streak;
    else
    {
      writeVariableInt(&wrapped, streak - relative);
      _data.push_back(ss ^ ps);

      while (true)
      {
        
        ss = source->read(&sbyte, 1);
        ps = patched->read(&pbyte, 1);

        _data.push_back(ss ^ ps);
      }
    }
  }

  return Status::Ok;
}

void Patch::test()
{
  file_data_source source(R"(C:\Users\Jack\Desktop\patapon\fe\source.gba)");
  file_data_source patchSource(R"(C:\Users\Jack\Desktop\patapon\fe\patch.ups)");
  file_data_sink sink(R"(C:\Users\Jack\Desktop\patapon\fe\patched.gba)");

  Patch patch;
  patch.load(&patchSource);
  patch.apply(&source, &sink);
}