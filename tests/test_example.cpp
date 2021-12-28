#include <doctest/doctest.h>
#include <spdlog/spdlog.h>
#include "illuminate/illuminate.h"
TEST_CASE("log") {
  spdlog::info("hello {}", "world");
  CHECK_UNARY(true);
}
