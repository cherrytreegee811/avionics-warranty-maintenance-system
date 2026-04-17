#pragma once
/**
 * @file DiagnosticState.h
 * @brief Declares the diagnostic mode concrete state.
 */

#include "BaseState.h"

namespace aircraft {

class DiagnosticState : public BaseState {
public:
  /**
   * @brief Constructs diagnostic state behavior.
   * @param aircraft Type: @ref aircraft::Aircraft&. Owning aircraft aggregate.
   * @param stateManager Type: @ref aircraft::StateManager&. Transition manager used by this state.
   */
  DiagnosticState(Aircraft& aircraft, StateManager& stateManager);

  /** @brief Runs periodic diagnostic-state update behavior. */
  void UpdateState() override;
  /** @brief Executes entry logic for diagnostic state. */
  void InitState() override;
  /** @brief Executes exit logic for diagnostic state. */
  void CleanUpState() override;

private:
  StateManager& m_stateManager;
};

}  // namespace aircraft
