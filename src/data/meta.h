#pragma once

#include <string>
#include <vector>

using ident_t = std::string;

namespace meta
{
  class Company
  {
  protected:
    ident_t _ident;
    std::string _name;

  public:
    Company(const ident_t& ident, const std::string& name) : _ident(ident), _name(name) { }

    const ident_t& ident() const { return _ident; }
    const std::string& name() const { return _name; };
  };
  
  class System
  {
  protected:
    ident_t _ident;
    std::string _shortName;
    std::string _longName;
    Company* _company;

  public:
    System(const ident_t& ident, Company* company, const std::string& shortName, const std::string& longName) 
      : _ident(ident), _company(company), _shortName(shortName), _longName(longName) { }

    const ident_t& ident() const { return _ident; }
    const std::string& shortName() const { return _shortName; }
    const std::string& longName() const { return _longName; }
  };

  class Repository
  {
  protected:
    std::vector<Company> _companies;
    std::vector<System> _systems;

  public:
    Repository();

    Company* company(const ident_t& ident);

    const std::vector<Company>& companies() const { return _companies; }
    const std::vector<System>& systems() const { return _systems; }

    static Repository* i();
  };
}