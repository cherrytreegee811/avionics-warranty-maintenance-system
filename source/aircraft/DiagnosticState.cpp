#include <aircraft/Aircraft.h>
#include <aircraft/DiagnosticState.h>

#include <iostream>

DiagnosticState::DiagnosticState(aircraft::Aircraft& aircraft, StateManager& stateManager)
    : BaseState(aircraft, stateManager), m_stateManager(stateManager) {}

void DiagnosticState::UpdateState() {}

void DiagnosticState::InitState() {
  m_aircraft.setCurrentState("DIAGNOSTIC");
  const bool diagnostic_sent = m_aircraft.sendDiagnosticData();
  const bool image_sent = m_aircraft.sendImageFromFile(
      "/home/technerd/Documents/Development/avionics-warranty-maintenance-system/res/"
      "Boeing737-800_diagram.png");

  if (diagnostic_sent && image_sent) {
    // After diagnostics are reported, move into maintenance so the server can
    // clear codes or escalate to FAULT if required by the current fault set.
    m_aircraft.transitionToState(network::StateId::MAINTENANCE,
                                 aircraft::TransitionSource::AUTOMATIC);
  }
}

void DiagnosticState::CleanUpState() {}