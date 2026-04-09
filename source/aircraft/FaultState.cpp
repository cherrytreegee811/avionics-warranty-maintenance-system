#include <aircraft/Aircraft.h>
#include <aircraft/FaultState.h>
#include <iostream>

FaultState::FaultState(aircraft::Aircraft& aircraft, StateManager& stateManager)
    : BaseState(aircraft, stateManager), m_stateManager(stateManager) {}

void FaultState::UpdateState() {}

void FaultState::InitState() { m_aircraft.setCurrentState("FAULT"); }

void FaultState::CleanUpState() {}
