#pragma once
/**
 * @file StateManager.h
 * @brief Declares the manager for aircraft state transitions.
 */

#include <memory>

#include "BaseState.h"

class StateManager {
private:
  std::unique_ptr<BaseState> m_currentState;

public:
  /** @brief Constructs an empty state manager. */
  StateManager();
  /**
   * @brief Immediately replaces the active state object.
   * @param newState Type: std::unique_ptr<@ref BaseState>. Newly active state instance.
   */
  void SetState(std::unique_ptr<BaseState> newState);
};