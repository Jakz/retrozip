#include "ips.h"

#include "tbx/streams/data_source.h"
#include "tbx/streams/memory_buffer.h"
#include "tbx/streams/file_data_source.h"
#include "tbx/streams/data_filter.h"
#include "filters/filters.h"

using namespace patch::ups;

/* read a variable int and shift pointer by given amount*/
uint64_t Patch::readVariableInt(const uint8_t*& ptr)
{
  uint64_t result = 0;
  uint64_t shift = 1;

  while (true)
  {
    uint8_t byte = *ptr++;
    result += (byte & 0x7F) * shift;
    if (byte & 0x80) /* stop bit */
      break;

    shift <<= 7;
    result += shift;

  }

  return result;
}

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

Status Patch::apply(seekable_data_source* source, data_sink* sink)
{
  bool verifySource = true;
  bool verifyOutput = true;
  
  bool finished = false;
  const uint8_t* data = _data.data();
  const uint8_t* dataStart = data;
  const uint8_t* dataEnd = data + _data.size();

  size_t sourceOffset = 0;
  size_t written = 0;

  memory_buffer buffer;

  if (source->size() != _header.inputSize)
    return Status::InvalidSourceSize;

  while (data < dataEnd)
  {
    /* read offset */
    auto* dataBefore = data;
    size_t amountToCopy = readVariableInt(data);
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

void Patch::test()
{
  file_data_source source(R"(C:\Users\Jack\Desktop\patapon\fe\source.gba)");
  file_data_source patchSource(R"(C:\Users\Jack\Desktop\patapon\fe\patch.ups)");
  file_data_sink sink(R"(C:\Users\Jack\Desktop\patapon\fe\patched.gba)");

  Patch patch;
  patch.load(&patchSource);
  patch.apply(&source, &sink);
}