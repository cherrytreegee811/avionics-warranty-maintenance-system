/**
 * @file StandbyState.cpp
 * @brief Implements standby mode state behavior.
 */

#include <aircraft/Aircraft.h>
#include <aircraft/DiagnosticState.h>
#include <aircraft/StandbyState.h>

#include <format>
#include <iostream>
#include <memory>

StandbyState::StandbyState(aircraft::Aircraft& aircraft, StateManager& stateManager)
    : BaseState(aircraft, stateManager), m_stateManager(stateManager) {}

void StandbyState::UpdateState() {
  // Standby state update logic - pauses and waits for input
  int menu_selection = 0;
  std::cout
      << "TESTING ONLY: Enter 1 to transition to Active State, or 0 to remain in Standby State: ";
  std::cin >> menu_selection;

  if (menu_selection == 1) {
    m_aircraft.transitionToState(network::StateId::DIAGNOSTIC, aircraft::TransitionSource::MANUAL);
  } else {
    std::cout << "Remaining in Standby State...\n";
  }
}

void StandbyState::InitState() { m_aircraft.setCurrentState("STANDBY"); }

void StandbyState::CleanUpState() {}
