/**
 * @file CliInterface.cpp
 * @brief Implements the aircraft command-line interface rendering and menus.
 */

#include <aircraft/CliInterface.h>

#include <chrono>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>

using namespace aircraft;

CliInterface::CliInterface(Aircraft& aircraft)
    : CliInterface(aircraft, std::cin, std::cout, true) {}

CliInterface::CliInterface(Aircraft& aircraft, std::istream& in, std::ostream& out,
                           bool enable_screen_control)
    : m_aircraft(aircraft), in_(in), out_(out), screen_control_enabled_(enable_screen_control) {}

void CliInterface::clearScreen() {
  if (!screen_control_enabled_) {
    return;
  }
// Cross-platform clear screen
#ifdef _WIN32
  system("cls");
#else
  system("clear");
#endif
}

void CliInterface::waitForEnter() {
  if (!screen_control_enabled_) {
    return;
  }
  out_ << "\nPress Enter to continue...";
  out_.flush();
  in_.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
  in_.get();
}

void CliInterface::printHeader(const std::string& title) {
  out_ << "\n========================================\n";
  out_ << "   " << title << "\n";
  out_ << "========================================\n";
}

void CliInterface::printSeparator() { out_ << "----------------------------------------\n"; }

std::string CliInterface::formatTimePoint(const std::chrono::system_clock::time_point& tp) const {
  auto time_t = std::chrono::system_clock::to_time_t(tp);
  std::stringstream ss;
  ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
  return ss.str();
}

void CliInterface::showMainMenu() {
  bool running = true;

  while (running) {
    clearScreen();
    printHeader("AIRCRAFT MAINTENANCE MANAGEMENT");

    // Display current state prominently
    out_ << "\nCurrent State: ";
    std::string state = m_aircraft.getCurrentState();
    if (state == "STANDBY") {
      out_ << "\033[32m";  // Green
    } else if (state == "DIAGNOSTIC") {
      out_ << "\033[33m";  // Yellow
    } else if (state == "MAINTENANCE") {
      out_ << "\033[36m";  // Cyan
    } else if (state == "FAULT") {
      out_ << "\033[31m";  // Red
    }
    out_ << state << "\033[0m\n";

    printSeparator();

    out_ << "\nPlease select an option:\n";
    out_ << "  1. View Current State\n";
    out_ << "  2. View Last Maintenance Time\n";
    out_ << "  3. View Fault Codes\n";
    out_ << "  4. View Warranty Status\n";
    out_ << "  5. View All Information\n";
    out_ << "  6. Exit\n";
    printSeparator();
    out_ << "Enter choice (1-6): ";
    out_.flush();

    int choice;
    in_ >> choice;

    if (in_.fail()) {
      in_.clear();
      in_.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
      out_ << "Invalid input. Please enter a number.\n";
      waitForEnter();
      continue;
    }

    switch (choice) {
      case 1:
        displayCurrentState();
        waitForEnter();
        break;
      case 2:
        displayLastMaintenance();
        waitForEnter();
        break;
      case 3:
        displayFaultCodes();
        waitForEnter();
        break;
      case 4:
        displayWarrantyStatus();
        waitForEnter();
        break;
      case 5:
        displayAllInfo();
        waitForEnter();
        break;
      case 6:
        out_ << "\nExiting application...\n";
        running = false;
        break;
      default:
        out_ << "Invalid choice. Please select 1-6.\n";
        waitForEnter();
    }
  }
}

void CliInterface::displayCurrentState() {
  clearScreen();
  printHeader("CURRENT AIRCRAFT STATE");
  out_ << "\nOperational State: " << m_aircraft.getCurrentState() << "\n";

  // Add state description
  std::string state = m_aircraft.getCurrentState();
  if (state == "STANDBY") {
    out_ << "\nDescription: Aircraft is on standby, awaiting commands.\n";
    out_ << "Available: Landed notification, diagnostic entry.\n";
  } else if (state == "DIAGNOSTIC") {
    out_ << "\nDescription: Aircraft is running diagnostics.\n";
    out_ << "Available: System checks, data transfer, schematic upload.\n";
  } else if (state == "MAINTENANCE") {
    out_ << "\nDescription: Aircraft is in maintenance mode.\n";
    out_ << "Available: Repairs, component replacement, system tests.\n";
  } else if (state == "FAULT") {
    out_ << "\nDescription: Aircraft has detected a fault.\n";
    out_ << "Action Required: Diagnostic check needed.\n";
  }
}

void CliInterface::displayLastMaintenance() {
  clearScreen();
  printHeader("LAST MAINTENANCE RECORD");

  MaintenanceInfo info = m_aircraft.getLastMaintenance();
  out_ << "\nDate/Time: " << formatTimePoint(info.lastMaintenance) << "\n";
  out_ << "Technician: " << info.technician << "\n";
  out_ << "Notes: " << info.notes << "\n";
}

void CliInterface::displayFaultCodes() {
  clearScreen();
  printHeader("FAULT CODES");

  auto faults = m_aircraft.getFaultCodes();
  if (faults.empty()) {
    out_ << "\nNo active fault codes.\n";
    out_ << "All systems operational.\n";
  } else {
    out_ << "\nActive Fault Codes:\n\n";
    for (const auto& fault : faults) {
      out_ << "  [CODE " << fault.code << "]\n";
      out_ << "    Severity: " << network::diagnosticFaultSeverityToString(fault.severity) << "\n";
      out_ << "    Description: " << fault.description << "\n";
      out_ << "    Detected: " << formatTimePoint(fault.timestamp) << "\n\n";
    }
  }
}

void CliInterface::displayWarrantyStatus() {
  clearScreen();
  printHeader("WARRANTY STATUS");

  WarrantyInfo info = m_aircraft.getWarranty();
  out_ << "\nStatus: ";
  if (info.isActive) {
    out_ << "\033[32mACTIVE\033[0m\n";
    out_ << "Expires: " << info.expiryDate << "\n";
    out_ << "Provider: " << info.provider << "\n";

    // Calculate days remaining
    out_ << "\nCoverage: Full parts and labor\n";
    out_ << "Contact: warranty@aviationcorp.com\n";
  } else {
    out_ << "\033[31mEXPIRED\033[0m\n";
    out_ << "Expired on: " << info.expiryDate << "\n";
    out_ << "\nPlease contact service center for renewal options.\n";
  }
}

void CliInterface::displayAllInfo() {
  clearScreen();
  printHeader("COMPLETE AIRCRAFT STATUS");

  // Current State
  out_ << "\n[STATE]\n";
  out_ << "  " << m_aircraft.getCurrentState() << "\n";

  // Maintenance
  out_ << "\n[MAINTENANCE]\n";
  MaintenanceInfo maint = m_aircraft.getLastMaintenance();
  out_ << "  Last Maintenance: " << formatTimePoint(maint.lastMaintenance) << "\n";
  out_ << "  Technician: " << maint.technician << "\n";

  // Warranty
  out_ << "\n[WARRANTY]\n";
  WarrantyInfo warranty = m_aircraft.getWarranty();
  out_ << "  Status: " << (warranty.isActive ? "ACTIVE" : "EXPIRED") << "\n";
  out_ << "  Expiry: " << warranty.expiryDate << "\n";
  out_ << "  Provider: " << warranty.provider << "\n";

  // Fault Codes
  out_ << "\n[FAULT CODES]\n";
  auto faults = m_aircraft.getFaultCodes();
  if (faults.empty()) {
    out_ << "  None detected\n";
  } else {
    for (const auto& fault : faults) {
      out_ << "  " << fault.code << " [" << network::diagnosticFaultSeverityToString(fault.severity)
           << "]: " << fault.description << "\n";
    }
  }

  printSeparator();
}