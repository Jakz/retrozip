#pragma once

#include <exception>
#include <string>

#include "path.h"

namespace args
{
  class ArgumentParser;
}

namespace exceptions
{
  class exception : public std::exception
  {
    
  };
  
  class messaged_exception : public exception
  {
  private:
    std::string _message;
    
  public:
    messaged_exception(const std::string& message) : _message(message) { }
    const char* what() const noexcept override { return _message.c_str(); }
  };
  
  class file_not_found : public exception
  {
  private:
    path _path;
    
  public:
    file_not_found(const class path& path) : _path(path) { }

    const char* what() const noexcept override { return _path.c_str(); }
  };
  
  class path_non_relative : public exception
  {
  private:
    path _parent;
    path _children;
    
  public:
    path_non_relative(const path& parent, const path& children) : _parent(parent), _children(children) { }
    
    const char* what() const noexcept override { return _parent.c_str(); }
  };
  
  class path_exception : public exception
  {
  private:
    std::string _message;
    
  public:
    path_exception(const std::string& message) : _message(message) { }
    
    const char* what() const noexcept override { return _message.c_str(); }
  };
  
  class error_opening_file : public exception
  {
  private:
    path _path;
    
  public:
    error_opening_file(const class path& path) : _path(path) { }
    
    const char* what() const noexcept override { return _path.c_str(); }
  };
  
  class error_reading_from_file : public exception
  {
  private:
    path _path;
    
  public:
    error_reading_from_file(const class path& path) : _path(path) { }
    
    const char* what() const noexcept override { return _path.c_str(); }
  };
  
  class parse_help_request : public exception
  {
  private:
    const args::ArgumentParser& _parser;
    
  public:
    parse_help_request(const args::ArgumentParser& parser) : _parser(parser) { }
    const char* what() const noexcept override { return nullptr; }
  };
  
  class not_enough_memory : public exception
  {
  private:
    const std::string _source;
    
  public:
    not_enough_memory(const std::string& source) : _source(source) { }
    const char* what() const noexcept override { return _source.c_str(); }
  };
  
  class unserialization_exception : public exception
  {
  private:
    const std::string _what;
    
  public:
    unserialization_exception(const std::string& what) : _what(what) { }
    const char* what() const noexcept override { return _what.c_str(); }
  };
  
  class missing_source_file_exception : public exception
  {
  private:
    const std::string _what;
    
  public:
    missing_source_file_exception(const std::string& what) : _what(what) { }
    const char* what() const noexcept override { return _what.c_str(); }
  };

  using file_format_error = messaged_exception;

}
