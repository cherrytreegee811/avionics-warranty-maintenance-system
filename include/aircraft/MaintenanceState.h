#pragma once

#include "BaseState.h"

namespace aircraft {
  class Aircraft;
}

class MaintenanceState : public BaseState {
public:
  MaintenanceState(aircraft::Aircraft& aircraft, StateManager& stateManager);

  void UpdateState() override;
  void DrawState() override;
  void InitState() override;
  void CleanUpState() override;

private:
  StateManager& m_stateManager;
};
