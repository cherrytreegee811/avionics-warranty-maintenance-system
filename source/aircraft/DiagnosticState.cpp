#include <aircraft/DiagnosticState.h>
#include <aircraft/Aircraft.h>
#include <iostream>

DiagnosticState::DiagnosticState(aircraft::Aircraft &aircraft, StateManager &stateManager)
    : BaseState(aircraft, stateManager), m_stateManager(stateManager) {}

void DiagnosticState::UpdateState() {
}

void DiagnosticState::DrawState() {
    std::cout << "Aircraft is in Diagnostic State\n";
}

void DiagnosticState::InitState() {
    std::cout << "Initializing Diagnostic State\n";
}

void DiagnosticState::CleanUpState() {
    std::cout << "Cleaning up Diagnostic State\n";
}
