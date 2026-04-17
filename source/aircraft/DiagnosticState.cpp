/**
 * @file DiagnosticState.cpp
 * @brief Implements diagnostic mode state behavior.
 */

#include <aircraft/Aircraft.h>
#include <aircraft/DiagnosticState.h>

#include <iostream>

namespace aircraft {

DiagnosticState::DiagnosticState(Aircraft& aircraft, StateManager& stateManager)
    : BaseState(aircraft, stateManager), m_stateManager(stateManager) {}

void DiagnosticState::InitState() {
  m_aircraft.setCurrentState("DIAGNOSTIC");
  const bool diagnostic_sent = m_aircraft.sendDiagnosticData();
  const bool warranty_sent = m_aircraft.sendWarrantyData();
  const bool image_sent = m_aircraft.sendImageFromFile("res/Boeing737-800_diagram.png");

  if (!warranty_sent) {
    std::cout << "Warning: warranty data did not transmit to MMA.\n";
  }

  if (!image_sent) {
    std::cout << "Warning: schematic image did not transmit to MMA.\n";
  }

  if (diagnostic_sent) {
    // After diagnostics are reported, move into maintenance so the server can
    // clear codes or escalate to FAULT if required by the current fault set.
    const bool transitioned = m_aircraft.transitionToState(network::StateId::MAINTENANCE,
                                                           TransitionSource::AUTOMATIC);
    if (!transitioned) {
      std::cout << "Warning: transition to MAINTENANCE was rejected.\n";
    }
  }
}

void DiagnosticState::CleanUpState() {}

}  // namespace aircraft