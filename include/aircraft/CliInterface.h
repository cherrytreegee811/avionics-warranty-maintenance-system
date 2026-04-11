#pragma once
/**
 * @file CliInterface.h
 * @brief Declares the aircraft command-line presentation interface.
 */

#include <aircraft/Aircraft.h>

#include <memory>
#include <string>

namespace aircraft {

  /**
   * @brief Console UI helper for displaying aircraft status and data.
   */
  class CliInterface {
  public:
    /** @brief Creates the interface bound to a specific aircraft model. */
    explicit CliInterface(Aircraft& aircraft);

    /** @brief Displays the top-level menu and command loop options. */
    void showMainMenu();
    /** @brief Prints current operational state. */
    void displayCurrentState();
    /** @brief Prints latest maintenance record. */
    void displayLastMaintenance();
    /** @brief Prints active fault code list. */
    void displayFaultCodes();
    /** @brief Prints warranty status summary. */
    void displayWarrantyStatus();
    /** @brief Prints full aircraft information snapshot. */
    void displayAllInfo();

  private:
    Aircraft& m_aircraft;

    void clearScreen();
    void waitForEnter();
    std::string formatTimePoint(const std::chrono::system_clock::time_point& tp) const;
    void printHeader(const std::string& title);
    void printSeparator();
  };

}  // namespace aircraft