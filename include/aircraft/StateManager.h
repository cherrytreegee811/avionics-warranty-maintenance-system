#pragma once
/**
 * @file StateManager.h
 * @brief Declares the manager for aircraft state transitions.
 */

#include <memory>

#include "BaseState.h"

namespace aircraft {

class StateManager {
private:
  std::unique_ptr<BaseState> m_currentState;

public:
  /** @brief Constructs an empty state manager. */
  StateManager();
  /**
   * @brief Immediately replaces the active state object.
   * @param newState Type: std::unique_ptr<@ref aircraft::BaseState>. Newly active state instance.
   */
  void SetState(std::unique_ptr<BaseState> newState);
<<<<<<< HEAD
};
=======
  /**
   * @brief Enqueues a state change request for later processing.
   * @param newState Type: std::unique_ptr<@ref aircraft::BaseState>. Pending state instance to
   * activate later.
   */
  void RequestStateChange(std::unique_ptr<BaseState> newState);
  /** @brief Processes pending transitions and updates active state. */
  void Update();
};

>>>>>>> origin/feature/misra
}  // namespace aircraft