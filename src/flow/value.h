#pragma once

#include <vector>
#include <variant>
#include <string>

#include "tbx/base/path.h"

namespace flow
{
  struct Value
  {
  public:
    using List = std::vector<Value>;

  protected:
    using data_t = std::variant<std::monostate, std::string, int, float, bool, path, List>;
    data_t _data;

  public:
    Value() : _data(std::monostate{}) { }
    template<typename T> Value(T&& value) : _data(value)
    {
    
    }

    template<typename T> bool is() const 
    {
      return std::holds_alternative<T>(_data);
    }

    template<typename T> const T* get() const
    {
      return std::get_if<T>(&_data);
    }

    bool empty() const { return is<std::monostate>(); }

    std::string_view typeName() const
    {
      if (is<std::string>()) return "string";
      else if (is<int>()) return "int";
      else if (is<float>()) return "float";
      else if (is<bool>()) return "bool";
      else if (is<path>()) return "path";
      else if (is<List>()) return "list";
      else return "unknown";
    }

    std::string caption() const
    {
      if (!empty())
      {
        if (is<std::string>())
          return *get<std::string>();
        else if (is<path>())
          return get<path>()->str();
        else if (is<int>())
          return std::to_string(*get<int>());
        else if (is<float>())
          return std::to_string(*get<float>());
        else
          return "unknown";
      }
      else
        return "empty";
    }
  };
}