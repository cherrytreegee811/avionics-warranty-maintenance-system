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
    /**
     * @brief Creates the interface bound to a specific aircraft model.
    * @param aircraft Type: @ref aircraft::Aircraft&. Aircraft model to display and control.
     */
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
    /**
     * @brief Formats a timestamp for console output.
    * @param tp Type: const std::chrono::system_clock::time_point&. Timestamp to format.
    * @return Type: std::string. Human-readable timestamp string.
     */
    std::string formatTimePoint(const std::chrono::system_clock::time_point& tp) const;
    /**
     * @brief Prints a section header.
    * @param title Type: const std::string&. Header text to print.
     */
    void printHeader(const std::string& title);
    void printSeparator();
  };

}  // namespace aircraft