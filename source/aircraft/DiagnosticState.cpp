/**
 * @file DiagnosticState.cpp
 * @brief Implements diagnostic mode state behavior.
 */

#include <aircraft/Aircraft.h>
#include <aircraft/DiagnosticState.h>

#include <iostream>

namespace aircraft {
<<<<<<< HEAD
=======

DiagnosticState::DiagnosticState(Aircraft& aircraft, StateManager& stateManager)
    : BaseState(aircraft, stateManager), m_stateManager(stateManager) {}
>>>>>>> origin/feature/misra

DiagnosticState::DiagnosticState(Aircraft& aircraft, StateManager& stateManager)
    : BaseState(aircraft, stateManager), m_stateManager(stateManager) {}

void DiagnosticState::InitState() {
  m_aircraft.setCurrentState("DIAGNOSTIC");
  const bool diagnostic_sent = m_aircraft.sendDiagnosticData();
  const bool warranty_sent = m_aircraft.sendWarrantyData();
  m_aircraft.sendImageFromFile("res/Boeing737-800_diagram.png");

  if (!warranty_sent) {
    std::cout << "Warning: warranty data did not transmit to MMA.\n";
  }

  if (diagnostic_sent) {
    // After diagnostics are reported, move into maintenance so the server can
    // clear codes or escalate to FAULT if required by the current fault set.
    m_aircraft.transitionToState(network::StateId::MAINTENANCE, TransitionSource::AUTOMATIC);
  }
}

void DiagnosticState::CleanUpState() {}

}  // namespace aircraft