#pragma once

#include <aircraft/Aircraft.h>

#include <memory>
#include <string>

namespace aircraft {

  class CliInterface {
  public:
    explicit CliInterface(Aircraft& aircraft);

    void showMainMenu();
    void displayCurrentState();
    void displayLastMaintenance();
    void displayFaultCodes();
    void displayWarrantyStatus();
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