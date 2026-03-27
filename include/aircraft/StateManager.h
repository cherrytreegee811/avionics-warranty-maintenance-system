#pragma once

#include "BaseState.h"
#include <memory>

#include <queue>

class StateManager {
private:
  std::unique_ptr<BaseState> m_currentState;
  std::queue<std::unique_ptr<BaseState>> m_stateQueue;

public:
  StateManager();
  void SetState(std::unique_ptr<BaseState> newState);
  void RequestStateChange(std::unique_ptr<BaseState> newState);
  void Update();
  void Draw();
};