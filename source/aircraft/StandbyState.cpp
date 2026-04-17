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

namespace aircraft {
<<<<<<< HEAD
=======

StandbyState::StandbyState(Aircraft& aircraft, StateManager& stateManager)
    : BaseState(aircraft, stateManager), m_stateManager(stateManager) {}
>>>>>>> origin/feature/misra

StandbyState::StandbyState(Aircraft& aircraft, StateManager& stateManager)
    : BaseState(aircraft, stateManager), m_stateManager(stateManager) {}

void StandbyState::InitState() { m_aircraft.setCurrentState("STANDBY"); }

void StandbyState::CleanUpState() {}

}  // namespace aircraft
