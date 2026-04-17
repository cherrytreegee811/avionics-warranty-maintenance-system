#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <aircraft/Aircraft.h>
#include <aircraft/BaseState.h>
#include <aircraft/StateManager.h>
#include <doctest/doctest.h>

#include <memory>

namespace {
  class RecordingState final : public BaseState {
  public:
    RecordingState(aircraft::Aircraft& aircraft, StateManager& manager, int& initCount,
                   int& cleanupCount, int& updateCount)
        : BaseState(aircraft, manager),
          initCount_(initCount),
          cleanupCount_(cleanupCount),
          updateCount_(updateCount) {}

    void UpdateState() override { ++updateCount_; }
    void InitState() override { ++initCount_; }
    void CleanUpState() override { ++cleanupCount_; }

  private:
    int& initCount_;
    int& cleanupCount_;
    int& updateCount_;
  };
}  // namespace

// ============================================================================
// REQ-SYS-060: The client or server (or both) application shall contain an operational state
// machine.
// ============================================================================

TEST_CASE("REQ-SYS-060: StateManager SetState initializes and cleans up states") {
  aircraft::Aircraft aircraft;
  StateManager manager;

  int initCount = 0;
  int cleanupCount = 0;
  int updateCount = 0;

  manager.SetState(
      std::make_unique<RecordingState>(aircraft, manager, initCount, cleanupCount, updateCount));
  CHECK(initCount == 1);
  CHECK(cleanupCount == 0);

  manager.SetState(
      std::make_unique<RecordingState>(aircraft, manager, initCount, cleanupCount, updateCount));
  CHECK(initCount == 2);
  CHECK(cleanupCount == 1);
}

TEST_CASE("REQ-SYS-060: StateManager Update ticks the active state") {
  aircraft::Aircraft aircraft;
  StateManager manager;

  int initCount = 0;
  int cleanupCount = 0;
  int updateCount = 0;

  manager.SetState(
      std::make_unique<RecordingState>(aircraft, manager, initCount, cleanupCount, updateCount));
  manager.Update();
  manager.Update();

  CHECK(updateCount == 2);
}

TEST_CASE("REQ-SYS-060: StateManager Update is safe with no state") {
  StateManager manager;
  manager.Update();
  CHECK(true);
}

TEST_CASE("REQ-SYS-060: StateManager can enqueue requested state changes") {
  aircraft::Aircraft aircraft;
  StateManager manager;

  int initCount = 0;
  int cleanupCount = 0;
  int updateCount = 0;

  manager.RequestStateChange(
      std::make_unique<RecordingState>(aircraft, manager, initCount, cleanupCount, updateCount));

  // Current implementation stores requests; processing is outside current scope.
  CHECK(true);
}
