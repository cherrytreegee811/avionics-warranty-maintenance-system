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

void StandbyState::UpdateState() {}

void StandbyState::InitState() { m_aircraft.setCurrentState("STANDBY"); }

void StandbyState::CleanUpState() {}
