#pragma once
/**
 * @file BaseState.h
 * @brief Defines the abstract state interface used by aircraft state machine states.
 */

namespace aircraft {
  class Aircraft;
}

class StateManager;

/**
 * @brief Base interface for all concrete aircraft operational states.
 */
class BaseState {
public:
  /**
   * @brief Constructs a state with references to owning aircraft and manager.
  * @param aircraft Type: @ref aircraft::Aircraft&. Owning aircraft aggregate.
  * @param stateManager Type: @ref StateManager&. Transition manager used by this state.
   */
  BaseState(aircraft::Aircraft& aircraft, StateManager& stateManager)
      : m_aircraft(aircraft), m_stateManager(stateManager) {};
  /** @brief Virtual destructor for polymorphic deletion. */
  virtual ~BaseState() {}
  /** @brief Executes one state update tick. */
  virtual void UpdateState() = 0;
  /** @brief Performs state entry initialization logic. */
  virtual void InitState() = 0;
  /** @brief Performs state exit cleanup logic. */
  virtual void CleanUpState() = 0;

protected:
  aircraft::Aircraft& m_aircraft;

private:
  StateManager& m_stateManager;
};