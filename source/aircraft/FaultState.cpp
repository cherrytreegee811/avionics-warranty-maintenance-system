#include <aircraft/Aircraft.h>
#include <aircraft/FaultState.h>

#include <iostream>

FaultState::FaultState(aircraft::Aircraft& aircraft, StateManager& stateManager)
    : BaseState(aircraft, stateManager), m_stateManager(stateManager) {}

void FaultState::UpdateState() {}

void FaultState::DrawState() { std::cout << "Aircraft is in Fault State\n"; }

void FaultState::InitState() { std::cout << "Initializing Fault State\n"; }

void FaultState::CleanUpState() { std::cout << "Cleaning up Fault State\n"; }
