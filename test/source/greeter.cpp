#include <doctest/doctest.h>
#include <aircraft/aircraft.h>
#include <mma/mma.h>

TEST_CASE("Aircraft") {
  using namespace aircraft;

  Aircraft aircraft;
  aircraft.initialize();
  CHECK(true);  // Stub test
}

TEST_CASE("MMA") {
  using namespace mma;

  MMA mma;
  mma.initialize();
  CHECK(true);  // Stub test
}
