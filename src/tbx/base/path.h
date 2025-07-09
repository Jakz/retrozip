#pragma once

#include <cassert>
#include <string>
#include <vector>
#include <unordered_set>
#include <functional>
#include <memory>
#include <filesystem>

#if _WIN32
#include <codecvt>
#endif

using path_extension = std::string;

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
  
  
  path() { }
  path(const char* data);
  path(std::string_view data);
  path(const std::string& data);
  path(const std::filesystem::path& path);

  std::vector<path> contents() const;
  static path current();
  
  static std::function<bool(path)> onlyFiles() { return [](const path& p) { return p.existsAsFile(); }; }
  static std::function<bool(path)> onlyFolders() { return [](const path& p) { return p.isFolder(); }; }

  bool isFolder() const;
  bool exists() const;
  bool existsAsFile() const;
  bool empty() const { return _data.empty(); }
  size_t length() const;
  
  path absolute() const;
  path relativizeToParent(const path& parent) const;
  path relativizeChildren(const path& children) const;
  std::filesystem::path fspath() const { return std::filesystem::path(_data); }
  
  path append(const path& other) const;
  path operator+(const path& other) const { return this->append(other); }
  path operator/(const path& other) const { return this->append(other); }

  inline path& operator+=(const path& other) { *this = append(other); return *this; }
  
  inline bool operator!=(const path& other) const { return !(_data == other._data); }
  inline bool operator==(const path& other) const { return _data == other._data; }
  
  bool isAbsolute() const;
  bool hasExtension(const std::string& ext) const;
  bool hasParent() const;
  
  path removeLast() const;
  path parent() const { return removeLast(); }
  path removeAllButFirst() const;
  std::pair<path, path> splitParentAndFilename() const;

  path makeRelative() const;
  
  std::string filename() const;
  std::string filenameWithoutExtension() const;
  path withExtension(const path_extension& extension) const;

  const std::string& data() const { return _data; }
  const std::string& str() const { return _data; }
  const char* c_str() const { return _data.c_str(); }
  
  friend std::ostream& operator<<(std::ostream& os, const class path& path) { os << path._data; return os; }
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

using path_filter = std::function<bool(const path&)>;

enum class file_mode
{
  WRITING,
  APPENDING,
  READING,

  Reading = READING,
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
    const char* smode = "rb";
    if (mode == file_mode::WRITING) smode = "wb+";
    else if (mode == file_mode::APPENDING) smode = "rb+";
    
    assert(!_file);
    _file = fopen(path.c_str() , smode);
    assert(_file);

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
    return std::filesystem::file_size(_path.data());
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

  std::vector<uint8_t> toBytes() const
  {
    size_t len = length();
    std::vector<uint8_t> data(len);
    read(data.data(), sizeof(uint8_t), len);
    close();
    return data;
  }

  operator bool() const { return _file != nullptr; }
  
  /*int fd() const
  {
    assert(_file);
    return fileno(_file);
  }*/
};

