#pragma once

namespace aircraft {
  class Aircraft;
}

class StateManager;

class BaseState {
public:
  BaseState(aircraft::Aircraft& aircraft, StateManager& stateManager)
      : m_aircraft(aircraft), m_stateManager(stateManager) {};
  virtual ~BaseState() {}
  virtual void UpdateState() = 0;
  virtual void InitState() = 0;
  virtual void CleanUpState() = 0;

protected:
  aircraft::Aircraft& m_aircraft;

private:
  StateManager& m_stateManager;
};