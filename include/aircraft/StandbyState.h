#pragma once

#include "BaseState.h"
#include "StateManager.h"

namespace aircraft {
  class Aircraft;
}

class StandbyState : public BaseState {
public:
  StandbyState(aircraft::Aircraft& aircraft, StateManager& stateManager);
  virtual ~StandbyState() {}

  void UpdateState() override;
  void DrawState() override;
  void InitState() override;
  void CleanUpState() override;

private:
  StateManager& m_stateManager;
};