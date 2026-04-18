#pragma once
/**
 * @file DiagnosticState.h
 * @brief Declares the diagnostic mode concrete state.
 */

#include "BaseState.h"

namespace aircraft {
  class Aircraft;
}

class DiagnosticState : public BaseState {
public:
  /**
   * @brief Constructs diagnostic state behavior.
   * @param aircraft Type: @ref aircraft::Aircraft&. Owning aircraft aggregate.
   * @param stateManager Type: @ref StateManager&. Transition manager used by this state.
   */
  DiagnosticState(aircraft::Aircraft& aircraft, StateManager& stateManager);

  /** @brief Executes entry logic for diagnostic state. */
  void InitState() override;
  /** @brief Executes exit logic for diagnostic state. */
  void CleanUpState() override;

private:
  StateManager& m_stateManager;
};
