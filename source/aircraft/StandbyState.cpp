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
  std::cout << "Enter 1 to transition to Active State, or 0 to remain in Standby State: ";
  std::cin >> menu_selection;

  if (menu_selection == 1) {
    m_stateManager.SetState(std::make_unique<DiagnosticState>(m_aircraft, m_stateManager));
  } else {
    std::cout << "Remaining in Standby State...\n";
  }
}

void StandbyState::DrawState() { std::cout << "Aircraft is in Standby State\n"; }

void StandbyState::InitState() {
  std::cout << "Initializing Standby State\n";
  std::cout << std::format("Aircraft token: {}\n", m_aircraft.token);
}

void StandbyState::CleanUpState() { std::cout << "Cleaning up Standby State\n"; }
