#pragma once

#include "data_source.h"
#include "base/path.h"

class file_data_source : public seekable_data_source
{
private:
  path _path;
  file_handle _handle;
  size_t _length;
  
public:
  file_data_source(const path& path, file_handle&& handle) : _path(path), _handle(handle), _length(handle.length()) { }
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
    
    size_t effective = _handle.read(dest, 1, amount);
    TRACE_F("%p: file_data_source::read(%lu/%lu)", this, _handle.tell(), _length);
    return effective;
  }
  
  void seek(off_t position) override
  {
    assert(_handle);
    TRACE_F("%p: file_data_source::seek(%lu)", this, position);

    _handle.seek(position, SEEK_SET);
  }
  
  off_t tell() const override
  {
    assert(_handle);
    return _handle.tell();
  }
  
  size_t size() const override
  {
    assert(_handle);
    return _length;
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
    {
      TRACE_F("%p: file_data_sink::write(%lu/%lu)", this, amount, _handle.tell()+amount);
      return _handle.write(src, 1, amount);
    }
    else
      return END_OF_STREAM;
  }
};

#include <list>
#include <unordered_map>

class paged_file_data_source : public seekable_data_source
{
private:
  size_t _pageSize;
  size_t _maxPages;
  size_t _cacheSize;
  std::list<size_t> _lru;
  std::unordered_map<size_t, std::unique_ptr<byte[]>> _pages;
  
  path _path;
  file_handle _handle;
  
  size_t _position;
  size_t _length;
  size_t _maxValidPages;
  
public:
  paged_file_data_source(const path& path, size_t pageSize, size_t maxPages, bool waitForOpen = false) :
  _pageSize(pageSize), _maxPages(maxPages),
  _path(path), _handle(waitForOpen ? file_handle(path) : file_handle(path, file_mode::READING)), _length(waitForOpen ? 0 : _handle.length()),
  _maxValidPages(waitForOpen ? 0 : (_length / _pageSize) + (_length % _pageSize ? 1 : 0)), _position(0)
  {

  }
  
  void open()
  {
    assert(!_handle);
    _handle.open(_path, file_mode::READING);
    _length = _handle.length();
    _maxValidPages = (_length / _pageSize) + (_length % _pageSize ? 1 : 0);
    _position = 0;
  }
  
  void seek(off_t offset) override { getPage(offset / _pageSize); _position = offset; }
  size_t size() const override { return _length; }
  off_t tell() const override { return _position; }
  
  size_t read(byte* dest, size_t amount) override
  {
    byte* page = getPage(_position / _pageSize);
    
    if (!page)
      return END_OF_STREAM;
    
    size_t positionInPage = _position % _pageSize;
    amount = std::min(amount, _pageSize - _position);
    
    memcpy(dest, page + positionInPage, amount);
    return amount;
  }
  
  byte* getPage(size_t index)
  {
    auto it = _pages.find(index);
    
    /* return page if cached */
    if (it != _pages.end())
      return it->second.get();

    /* free one slot from cache if already full */
    if (_cacheSize == _maxPages)
    {
      size_t least = _lru.front();
      _lru.pop_front();
      _pages.erase(least);
    }
    
    /* if page is outside file dimension then don't do anything */
    if (index >= _maxValidPages)
      return nullptr;
    
    byte* page = new byte[_pageSize];
    _handle.seek(_pageSize * index, SEEK_CUR);
    _handle.read(page, 1, _pageSize);
    _pages.emplace(std::make_pair(index, std::unique_ptr<byte[]>(page)));
    _lru.push_back(index);
    
    return page;
  }
  
  size_t sizeInMemory() const { return _pageSize * _pages.size(); }
};
