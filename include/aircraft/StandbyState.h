#pragma once
/**
 * @file StandbyState.h
 * @brief Declares the standby mode concrete state.
 */

#include "BaseState.h"
#include "StateManager.h"

namespace aircraft {

class StandbyState : public BaseState {
public:
  /**
   * @brief Constructs standby state behavior.
   * @param aircraft Type: @ref aircraft::Aircraft&. Owning aircraft aggregate.
   * @param stateManager Type: @ref aircraft::StateManager&. Transition manager used by this state.
   */
  StandbyState(Aircraft& aircraft, StateManager& stateManager);
  /** @brief Virtual destructor for polymorphic cleanup. */
  ~StandbyState() override = default;

  /** @brief Runs periodic standby-state update behavior. */
  void UpdateState() override;
  /** @brief Executes entry logic for standby state. */
  void InitState() override;
  /** @brief Executes exit logic for standby state. */
  void CleanUpState() override;

private:
  StateManager& m_stateManager;
};

}  // namespace aircraft