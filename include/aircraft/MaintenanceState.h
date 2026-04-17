#pragma once
/**
 * @file MaintenanceState.h
 * @brief Declares the maintenance mode concrete state.
 */

#include "BaseState.h"

namespace aircraft {
  class Aircraft;
}

class MaintenanceState : public BaseState {
public:
  /**
   * @brief Constructs maintenance state behavior.
   * @param aircraft Type: @ref aircraft::Aircraft&. Owning aircraft aggregate.
   * @param stateManager Type: @ref StateManager&. Transition manager used by this state.
   */
  MaintenanceState(aircraft::Aircraft& aircraft, StateManager& stateManager);

  /** @brief Executes entry logic for maintenance state. */
  void InitState() override;
  /** @brief Executes exit logic for maintenance state. */
  void CleanUpState() override;

private:
  StateManager& m_stateManager;
};
