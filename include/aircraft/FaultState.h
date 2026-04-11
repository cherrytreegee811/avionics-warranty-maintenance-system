#pragma once
/**
 * @file FaultState.h
 * @brief Declares the fault mode concrete state.
 */

#include "BaseState.h"

namespace aircraft {
  class Aircraft;
}

class FaultState : public BaseState {
public:
  /** @brief Constructs fault state behavior. */
  FaultState(aircraft::Aircraft& aircraft, StateManager& stateManager);

  /** @brief Runs periodic fault-state update behavior. */
  void UpdateState() override;
  /** @brief Executes entry logic for fault state. */
  void InitState() override;
  /** @brief Executes exit logic for fault state. */
  void CleanUpState() override;

private:
  StateManager& m_stateManager;
};
