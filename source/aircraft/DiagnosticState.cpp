#include <aircraft/Aircraft.h>
#include <aircraft/DiagnosticState.h>

#include <iostream>

DiagnosticState::DiagnosticState(aircraft::Aircraft& aircraft, StateManager& stateManager)
    : BaseState(aircraft, stateManager), m_stateManager(stateManager) {}

void DiagnosticState::UpdateState() {}

void DiagnosticState::InitState() {
  m_aircraft.setCurrentState("DIAGNOSTIC");
  if (m_aircraft.sendWarrantyData() && m_aircraft.sendDiagnosticData()) {
    m_aircraft.transitionToState(network::StateId::MAINTENANCE);
  const bool diagnostic_sent = m_aircraft.sendDiagnosticData();
  m_aircraft.sendImageFromFile("res/Boeing737-800_diagram.png");

  if (diagnostic_sent) {
    // After diagnostics are reported, move into maintenance so the server can
    // clear codes or escalate to FAULT if required by the current fault set.
    m_aircraft.transitionToState(network::StateId::MAINTENANCE,
                                 aircraft::TransitionSource::AUTOMATIC);
  }
}

void DiagnosticState::CleanUpState() {}