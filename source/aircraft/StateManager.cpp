#include <aircraft/StateManager.h>
#include <aircraft/BaseState.h>

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

void StateManager::RequestStateChange(std::unique_ptr<BaseState> newState) {
  m_stateQueue.push(std::move(newState));
}

void StateManager::Update() {
  if (m_currentState) {
    m_currentState->UpdateState();
  }
}

void StateManager::Draw() {
  if (m_currentState) {
    m_currentState->DrawState();
  }
}
