#include <iostream>

#include "hash.h"

#include "base/common.h"
#include "base/exceptions.h"

namespace hash
{
  static constexpr size_t TABLE_SIZE = 256;
  static constexpr u32 POLYNOMIAL = 0xEDB88320;
  u32 lut[TABLE_SIZE] = {0};

  void crc32_digester::precomputeLUT()
  {
    bool init = false;

    if (!init)
    {
      for (u32 i = 0; i < 256; ++i)
      {
        u32 crc = i;
        for (u32 j = 0; j < 8; j++)
          crc = (crc >> 1) ^ (-int(crc & 1) & POLYNOMIAL);
        
        lut[i] = crc;
      }
      
      init = true;
    }
  }
  
  crc32_t crc32_digester::update(const void* data, size_t length, crc32_t previous)
  {
    u32 crc = ~previous;
    const byte* bdata = reinterpret_cast<const byte*>(data);
    
    while (length--)
      crc = (crc >> 8) ^ lut[(crc & 0xFF) ^ *bdata++];
    
    return ~crc;
  }

  void crc32_digester::update(const void* data, size_t length)
  {
    value = update(data, length, value);
  }
  
  crc32_t crc32_digester::compute(const void* data, size_t length)
  {
    crc32_digester digester;
    digester.update(data, length);
    return digester.get();
  }

  crc32_t crc32_digester::compute(const class path& path)
  {
    if (!path.exists())
      throw exceptions::file_not_found(path);
    
    file_handle handle = file_handle(path, file_mode::READING);
    
    if (!handle)
      throw exceptions::error_opening_file(path);
    
    size_t fileLength = handle.length();
    size_t current = 0;
    std::unique_ptr<byte[]> bufferPtr = std::unique_ptr<byte[]>(new byte[KB16]);
    byte* buffer = bufferPtr.get();
    
    crc32_digester digester;
    
    while (current < fileLength)
    {
      size_t amountToProcess = std::min(KB16, fileLength - current);
      
      if (handle.read(buffer, 1, amountToProcess))
        digester.update(buffer, amountToProcess);
      else
        throw exceptions::error_reading_from_file(path);
      
      current += KB16;
    }

    return digester.get();
  }
}
