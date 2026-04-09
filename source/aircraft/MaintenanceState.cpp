#include <aircraft/Aircraft.h>
#include <aircraft/MaintenanceState.h>
#include <iostream>

MaintenanceState::MaintenanceState(aircraft::Aircraft& aircraft, StateManager& stateManager)
    : BaseState(aircraft, stateManager), m_stateManager(stateManager) {}

void MaintenanceState::UpdateState() {}

void MaintenanceState::InitState() { m_aircraft.setCurrentState("MAINTENANCE"); }

void MaintenanceState::CleanUpState() {}
