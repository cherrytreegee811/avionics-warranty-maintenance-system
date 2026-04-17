/**
 * @file StateManager.cpp
 * @brief Implements state transition management.
 */

#include <aircraft/BaseState.h>
#include <aircraft/StateManager.h>

StateManager::StateManager() : m_currentState(nullptr) {}

void StateManager::SetState(std::unique_ptr<BaseState> newState) {
  if (m_currentState) {
    m_currentState->CleanUpState();
  }
  m_currentState = std::move(newState);
  if (m_currentState) {
    m_currentState->InitState();
  }
}
