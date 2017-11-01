#pragma once

#include "data_source.h"
#include "base/path.h"

class file_data_source : public data_source
{
private:
  path _path;
  file_handle _handle;
  size_t _length;
  
public:
  file_data_source(path path, bool waitForOpen = false) : _path(path), _handle(waitForOpen ? file_handle(path) : file_handle(path, file_mode::READING)), _length(waitForOpen ? 0 : _handle.length()) { }
  
  void open()
  {
    assert(!_handle);
    _handle.open(_path, file_mode::READING);
    _length = _handle.length();
  }
  
  size_t read(byte* dest, size_t amount) override
  {
    if (_handle.tell() == _length)
      return END_OF_STREAM;
    
    return _handle.read(dest, 1, amount);
  }
};

class file_data_sink : public data_sink
{
private:
  path _path;
  file_handle _handle;
  
public:
  file_data_sink(path path, bool waitForOpen = false) : _path(path), _handle(waitForOpen ? file_handle(path) : file_handle(path, file_mode::WRITING)) { }
    
  void open()
  {
    assert(!_handle);
    _handle.open(_path, file_mode::WRITING);
  }
  
  size_t write(const byte* src, size_t amount) override
  {
    if (amount != END_OF_STREAM)
      return _handle.write(src, 1, amount);
    else
      return END_OF_STREAM;
  }

};
