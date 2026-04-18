#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <aircraft/Aircraft.h>
#include <aircraft/BaseState.h>
#include <aircraft/StateManager.h>
#include <doctest/doctest.h>

#include <memory>

namespace {
  class RecordingState final : public aircraft::BaseState {
  public:
    RecordingState(aircraft::Aircraft& aircraft, aircraft::StateManager& manager, int& initCount,
                   int& cleanupCount)
        : aircraft::BaseState(aircraft, manager),
          initCount_(initCount),
          cleanupCount_(cleanupCount) {}

    void InitState() override { ++initCount_; }
    void CleanUpState() override { ++cleanupCount_; }

  private:
    int& initCount_;
    int& cleanupCount_;
  };
}  // namespace

// ============================================================================
// REQ-SYS-060: The client or server (or both) application shall contain an operational state
// machine.
// ============================================================================

TEST_CASE("REQ-SYS-060: StateManager SetState initializes and cleans up states") {
  aircraft::Aircraft aircraft;
  aircraft::StateManager manager;

  int initCount = 0;
  int cleanupCount = 0;

  manager.SetState(std::make_unique<RecordingState>(aircraft, manager, initCount, cleanupCount));
  CHECK(initCount == 1);
  CHECK(cleanupCount == 0);

  manager.SetState(std::make_unique<RecordingState>(aircraft, manager, initCount, cleanupCount));
  CHECK(initCount == 2);
  CHECK(cleanupCount == 1);
}
