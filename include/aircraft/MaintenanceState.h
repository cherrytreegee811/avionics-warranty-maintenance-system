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
  /** @brief Constructs maintenance state behavior. */
  MaintenanceState(aircraft::Aircraft& aircraft, StateManager& stateManager);

  /** @brief Runs periodic maintenance-state update behavior. */
  void UpdateState() override;
  /** @brief Executes entry logic for maintenance state. */
  void InitState() override;
  /** @brief Executes exit logic for maintenance state. */
  void CleanUpState() override;

private:
  StateManager& m_stateManager;
};
