#include <aircraft/Aircraft.h>
#include <aircraft/MaintenanceState.h>

#include <iostream>

MaintenanceState::MaintenanceState(aircraft::Aircraft& aircraft, StateManager& stateManager)
    : BaseState(aircraft, stateManager), m_stateManager(stateManager) {}

void MaintenanceState::UpdateState() {}

void MaintenanceState::DrawState() { std::cout << "Aircraft is in Maintenance State\n"; }

void MaintenanceState::InitState() { std::cout << "Initializing Maintenance State\n"; }

void MaintenanceState::CleanUpState() { std::cout << "Cleaning up Maintenance State\n"; }
