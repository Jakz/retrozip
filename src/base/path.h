#pragma once

#include <cassert>
#include <string>
#include <unordered_set>
#include <functional>

#if _WIN32
#include <codecvt>
#endif

class path
{
private:
  std::string _data;
  
public:
  using predicate = std::function<bool(const path&)>;
  struct hash
  {
  public:
    size_t operator()(const path& p) const { return std::hash<std::string>()(p._data); }
  };
  
  
  path(const char* data) : _data(data) { }
  path(const std::string& data) : _data(data) { }
  
  bool exists() const;
  size_t length() const;
  
  path relativizeToParent(const path& parent) const;
  path relativizeChildren(const path& children) const;
  path append(const path& other) const;
  
  bool operator==(const path& other) const { return _data == other._data; }
  
  bool hasExtension(const std::string& ext) const;

  
  const char* c_str() const { return _data.c_str(); }
  
  friend std::ostream& operator<<(std::ostream& os, const class path& path) { os << path._data; return os; }
  
  static std::unordered_set<path, hash> scanFolder(path base, bool recursive, predicate excludePredicate);
};

class relative_path
{
private:
  path _parent;
  path _child;
  
public:
  struct hash
  {
  public:
    size_t operator()(const relative_path& p) const { return path::hash()(p._parent.append(p._child)); }
  };
  
  relative_path(const path& parent, const path& child) : _parent(parent), _child(child) { }
  
  const path& parent() const { return _parent; }
  const path& child() const { return _child; }
  
  bool operator==(const path& path) const { return _parent.append(_child) == path; }
  bool operator==(const relative_path& other) const { return _parent == other._parent && _child == other._child; }
};

enum class file_mode
{
  WRITING,
  APPENDING,
  READING
};


class file_handle
{
private:
  path _path;
  mutable FILE* _file;

public:
  static bool read(void* dest, size_t size, size_t count, const file_handle& handle) { return handle.read(dest, size, count); }
  static bool write(const void* src, size_t size, size_t count, const file_handle& handle) { return handle.write(src, size, count); }
  
  file_handle(const class path& path) : _path(path), _file(nullptr) { }
  file_handle(const class path& path, file_mode mode, bool readOnly = false) : _file(nullptr), _path(path)
  {
    open(path, mode);
  }
  
  ~file_handle() { if (_file) close(); }
  
  file_handle& operator=(file_handle& other) { this->_file = other._file; this->_path = other._path; other._file = nullptr; return *this; }
  file_handle(const file_handle& other) : _file(other._file), _path(other._path) { other._file = nullptr; }
  
  template <typename T> bool write(const T& src) const { return write(&src, sizeof(T), 1); }
  template <typename T> bool read(T& dst) const { return read(&dst, sizeof(T), 1); }
  
  size_t write(const void* ptr, size_t size, size_t count) const {
    assert(_file);
    size_t r = fwrite(ptr, size, count, _file);
    return r;
  }
  
  size_t read(void* ptr, size_t size, size_t count) const {
    assert(_file);
    size_t r = fread(ptr, size, count, _file);
    return r;
  }
  
  void seek(long offset, int origin) const {
    assert(_file);
    fseek(_file, offset, origin);
  }
  
  long tell() const {
    assert(_file);
    return ftell(_file);
  }
  
  void rewind() const { fseek(_file, 0, SEEK_SET); }
  void flush() const { fflush(_file); }
  
  bool open(const class path& path, file_mode mode)
  {
#ifdef _WIN32
    const wchar_t* smode = L"rb";
    if (mode == file_mode::WRITING) smode = L"wb+";
    else if (mode == file_mode::APPENDING) smode = L"rb+";
    
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> conv;
    std::wstring wpath = conv.from_bytes(path.c_str());
    _file = _wfopen(wpath.c_str(), smode);
#else
    const char* smode = "rb";
    if (mode == file_mode::WRITING) smode = "wb+";
    else if (mode == file_mode::APPENDING) smode = "rb+";
    
    _file = fopen(path.c_str() , smode);
#endif
    
    //if (!file || ferror(file))
    //  printf("FILE* %s (mode: %s) error: (%d) %s\n", path.c_str(), smode, errno, strerror(errno));
    
    return _file != nullptr;
  }

  bool close() const
  {
    if (_file == nullptr)
      assert(false);
    else
    {
      fclose(_file);
      _file = nullptr;
    }
    
    return true;
  }
  
  size_t length() const
  {
    assert(_file != nullptr);
    long c = ftell(_file);
    fseek(_file, 0, SEEK_END);
    long s = ftell(_file);
    fseek(_file, c, SEEK_SET);
    return s;
  }
  
  std::string toString()
  {
    size_t len = length();
    std::unique_ptr<char[]> data = std::unique_ptr<char[]>(new char[len+1]);
    read(data.get(), sizeof(char), len);
    data.get()[len] = '\0';
    close();
    return std::string(data.get());
  }

  operator bool() const { return _file != nullptr; }
};
