#pragma once

#include "data_source.h"
#include "base/path.h"

class file_data_source : public data_source
{
private:
  path _path;
  file_handle _handle;
  
public:
  file_data_source(path path, bool waitForOpen = false) : _path(path), _handle(waitForOpen ? file_handle(path) : file_handle(path, file_mode::READING)) { }
  
  void open()
  {
    assert(!_handle);
    _handle.open(_path, file_mode::READING);
  }
  
  bool eos() const override
  {
    //TODO: maybe cache length?
    return _handle.tell() == _handle.length();
  }
  
  size_t read(void* dest, size_t size, size_t count) const override
  {
    return _handle.read(dest, size, count);
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
  
  size_t write(const void* src, size_t size, size_t count)
  {
    return _handle.write(src, size, count);
  }

};
