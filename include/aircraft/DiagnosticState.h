#pragma once

#include "BaseState.h"

namespace aircraft {
  class Aircraft;
}

class DiagnosticState : public BaseState {
public:
  DiagnosticState(aircraft::Aircraft& aircraft, StateManager& stateManager);

  void UpdateState() override;
  void DrawState() override;
  void InitState() override;
  void CleanUpState() override;

private:
  StateManager& m_stateManager;
};
