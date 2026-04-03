#include <aircraft/Aircraft.h>
#include <aircraft/DiagnosticState.h>

#include <iostream>

DiagnosticState::DiagnosticState(aircraft::Aircraft& aircraft, StateManager& stateManager)
    : BaseState(aircraft, stateManager), m_stateManager(stateManager) {}

void DiagnosticState::UpdateState() {}

void DiagnosticState::InitState() {
  m_aircraft.setCurrentState("DIAGNOSTIC");
  std::cout << "Initializing Diagnostic State\n";
  m_aircraft.sendDiagnosticData();
}

void DiagnosticState::CleanUpState() { std::cout << "Cleaning up Diagnostic State\n"; }
