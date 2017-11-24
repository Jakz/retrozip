#define CATCH_CONFIG_MAIN

#define CATCH_CONFIG_FAST_COMPILE

#include "libs/catch.h"
#include "libs/fmt/printf.h"
#include "cli/args.h"

using namespace args;

TEST_CASE("dummy")
{
  Parser p;
  p.print();
}

TEST_CASE("no arguments")
{
  Parser p;
  Arguments a = p.parse({});
  REQUIRE(!a);
  REQUIRE(a.error.message == error::missing_command);
}

TEST_CASE("unknown command")
{
  const std::string bogus = "foo";
  Parser p;
  Arguments a = p.parse({ bogus });
  REQUIRE(!a);
  REQUIRE(a.error.message == fmt::sprintf(error::unknown_command, bogus));
}

