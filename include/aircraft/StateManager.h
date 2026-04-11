#pragma once
/**
 * @file StateManager.h
 * @brief Declares the queue-driven manager for aircraft state transitions.
 */

#include <memory>
#include <queue>

#include "BaseState.h"

class StateManager {
private:
  std::unique_ptr<BaseState> m_currentState;
  std::queue<std::unique_ptr<BaseState>> m_stateQueue;

public:
  /** @brief Constructs an empty state manager. */
  StateManager();
  /** @brief Immediately replaces the active state object. */
  void SetState(std::unique_ptr<BaseState> newState);
  /** @brief Enqueues a state change request for later processing. */
  void RequestStateChange(std::unique_ptr<BaseState> newState);
  /** @brief Processes pending transitions and updates active state. */
  void Update();
};