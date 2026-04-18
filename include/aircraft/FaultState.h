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
  /**
   * @brief Constructs fault state behavior.
   * @param aircraft Type: @ref aircraft::Aircraft&. Owning aircraft aggregate.
   * @param stateManager Type: @ref StateManager&. Transition manager used by this state.
   */
  FaultState(aircraft::Aircraft& aircraft, StateManager& stateManager);

  /** @brief Executes entry logic for fault state. */
  void InitState() override;
  /** @brief Executes exit logic for fault state. */
  void CleanUpState() override;

private:
  StateManager& m_stateManager;
};
