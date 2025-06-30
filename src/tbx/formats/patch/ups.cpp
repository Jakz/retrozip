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

Status Patch::write(data_sink* base)
{
  auto digester = unbuffered_sink_filter<filters::crc32_filter>(base);

  enriched_data_sink sink(&digester);
  
  /* write header */
  sink.write(_header.magic, 4);
  writeVariableInt(&sink, _header.inputSize);
  writeVariableInt(&sink, _header.outputSize);
  
  /* write data */
  sink.write(_data.data(), _data.size());

  /* write checksums */
  sink.write((const byte*)&_checksum.inputChecksum, sizeof(uint32_t));
  sink.write((const byte*)&_checksum.outputChecksum, sizeof(uint32_t));
  _checksum.patchChecksum = digester.filter().get();
  sink.write((const byte*)&_checksum.patchChecksum, sizeof(uint32_t));
  return Status::Ok;
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

  weak_data_source weakData = weak_data_source(_data);

  size_t sourceOffset = 0;
  size_t written = 0;

  memory_buffer buffer;

  if (source->size() != _header.inputSize)
    return Status::InvalidSourceSize;

  /* special case: empty patch */
  if (_data.empty())
  {
    size_t finalSize = _header.outputSize;
    size_t toBeCopied = std::min(_header.inputSize, _header.outputSize);
    buffer.ensure_capacity(finalSize);

    source->read(buffer.direct(), toBeCopied);
    
    if (toBeCopied < finalSize)
      memset(buffer.direct() + toBeCopied, 0, finalSize - toBeCopied);

    sink->write(buffer.direct(), finalSize);
    return Status::Ok;
  }

  while (!weakData.eos())
  {
    /* read offset */
    size_t dataBefore = weakData.tell();
    size_t amountToCopy = readVariableInt(&weakData);
    size_t offset = 0;
    //offset += sourceOffset;

    /* copy bytes until offset is correct */
    //size_t amountToCopy = offset - sourceOffset;
    TRACE("%p: Patch::apply: %08x sourceOffset = %08x -> %08x, amountToCopy = %lu", this, _header.headerSizeInBytes + dataBefore, sourceOffset, sourceOffset + amountToCopy, amountToCopy);

    buffer.ensure_capacity(amountToCopy + 1);
    buffer.seek(0);
    memset(buffer.direct(), 0, amountToCopy);

    auto actual = source->read(buffer.direct(), amountToCopy);

    sink->write(buffer.direct(), amountToCopy);

    written += amountToCopy;
    sourceOffset += amountToCopy;

    size_t before = written;

    /* now keep xoring until a byte it's equal both on source and patch */
    do
    {
      dataBefore = weakData.tell();
      uint8_t patchByte;
      weakData.read(&patchByte, 1);
      
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
        TRACE("%p: Patch::apply: %08x written = %lu, sourceOffset = %08x", this, _header.headerSizeInBytes + dataBefore, written - before, sourceOffset);
        break;
      }

      if (weakData.eos())
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

Status Patch::generate(seekable_data_source* sourcer, seekable_data_source* patchedr)
{  
  auto wrapped = weak_vector_sink(_data);
  
  bool finished = false;

  auto source = unbuffered_source_filter<filters::crc32_filter>(sourcer);
  auto patched = unbuffered_source_filter<filters::crc32_filter>(patchedr);

  size_t totalOutput = std::max(sourcer->size(), patchedr->size());
  size_t offset = 0;
  size_t relative = 0;

  std::memcpy(_header.magic, "UPS1", 4);
  _header.inputSize = sourcer->size();
  _header.outputSize = patchedr->size();

  while (offset < totalOutput)
  {
    uint8_t sbyte, pbyte;
    
    /* read one byte from source and patched */
    source.read(&sbyte, 1);
    patched.read(&pbyte, 1);

    /* they're equal = skip */
    if (sbyte == pbyte)
      ++offset;
    else
    {
      /* fist byte different, we need to generate a xor block */

      TRACE("%p: Patch::generate: %08x -> %08x copied = %lu", this, relative, offset, offset - relative);
      writeVariableInt(&wrapped, offset - relative);
      relative = offset;
      _data.push_back(sbyte ^ pbyte);
      ++offset;

      while (true)
      {
        if (offset >= totalOutput)
        {
          _data.push_back(0x00);
          break;
        }

        /* keep going on xor block until we find to bytes equal */
        source.read(&sbyte, 1);
        patched.read(&pbyte, 1);
        ++offset;

        _data.push_back(sbyte ^ pbyte);

        if (sbyte == pbyte)
        {
          TRACE("%p: Patch::generate: %08x -> %08x xored = %lu", this, relative, offset, offset - relative);
          break;
        }
      }

      relative = offset;
    }
  }

  _checksum.inputChecksum = source.filter().get();
  _checksum.outputChecksum = patched.filter().get();
  /* patch checksum is written when serializing */

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
