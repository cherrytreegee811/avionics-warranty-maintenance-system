#include <aircraft/Aircraft.h>
#include <aircraft/DiagnosticState.h>

#include <iostream>

DiagnosticState::DiagnosticState(aircraft::Aircraft& aircraft, StateManager& stateManager)
    : BaseState(aircraft, stateManager), m_stateManager(stateManager) {}

void DiagnosticState::UpdateState() {}

void DiagnosticState::InitState() {
  m_aircraft.setCurrentState("DIAGNOSTIC");
  m_aircraft.sendDiagnosticData();
  m_aircraft.sendImageFromFile(
      "/home/technerd/Documents/Development/avionics-warranty-maintenance-system/res/"
      "Boeing737-800_diagram.png");
}

void DiagnosticState::CleanUpState() {}