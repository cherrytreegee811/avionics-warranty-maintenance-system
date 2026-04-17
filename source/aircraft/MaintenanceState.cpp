/**
 * @file MaintenanceState.cpp
 * @brief Implements maintenance mode state behavior.
 */

#include <aircraft/Aircraft.h>
#include <aircraft/MaintenanceState.h>

#include <iostream>

namespace aircraft {

MaintenanceState::MaintenanceState(Aircraft& aircraft, StateManager& stateManager)
    : BaseState(aircraft, stateManager), m_stateManager(stateManager) {}

void MaintenanceState::UpdateState() {}

void MaintenanceState::InitState() { m_aircraft.setCurrentState("MAINTENANCE"); }

void MaintenanceState::CleanUpState() {}

}  // namespace aircraft
