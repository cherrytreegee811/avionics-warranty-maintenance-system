#pragma once
/**
 * @file StandbyState.h
 * @brief Declares the standby mode concrete state.
 */

#include "BaseState.h"
#include "StateManager.h"

namespace aircraft {
  class Aircraft;
}

class StandbyState : public BaseState {
public:
  /** @brief Constructs standby state behavior. */
  StandbyState(aircraft::Aircraft& aircraft, StateManager& stateManager);
  /** @brief Virtual destructor for polymorphic cleanup. */
  virtual ~StandbyState() {}

  /** @brief Runs periodic standby-state update behavior. */
  void UpdateState() override;
  /** @brief Executes entry logic for standby state. */
  void InitState() override;
  /** @brief Executes exit logic for standby state. */
  void CleanUpState() override;

private:
  StateManager& m_stateManager;
};