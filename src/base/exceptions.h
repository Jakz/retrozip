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
  class rzip_exception : public std::exception
  {
    
  };
  
  class file_not_found : public rzip_exception
  {
  private:
    path path;
    
  public:
    file_not_found(const class path& path) : path(path) { }

    const char* what() const noexcept override { return path.c_str(); }
  };
  
  class path_non_relative : public rzip_exception
  {
  private:
    path parent;
    path children;
    
  public:
    path_non_relative(const path& parent, const path& children) : parent(parent), children(children) { }
    
    const char* what() const noexcept override { return parent.c_str(); }
  };
  
  class error_opening_file : public rzip_exception
  {
  private:
    path path;
    
  public:
    error_opening_file(const class path& path) : path(path) { }
    
    const char* what() const noexcept override { return path.c_str(); }
  };
  
  class error_reading_from_file : public rzip_exception
  {
  private:
    path path;
    
  public:
    error_reading_from_file(const class path& path) : path(path) { }
    
    const char* what() const noexcept override { return path.c_str(); }
  };
  
  class parse_help_request : public rzip_exception
  {
  private:
    const args::ArgumentParser& parser;
    
  public:
    parse_help_request(const args::ArgumentParser& parser) : parser(parser) { }
    const char* what() const noexcept override { return nullptr; }
  };
  
  class not_enough_memory : public rzip_exception
  {
  private:
    const std::string source;
    
  public:
    not_enough_memory(const std::string& source) : source(source) { }
    const char* what() const noexcept override { return source.c_str(); }
  };
}
