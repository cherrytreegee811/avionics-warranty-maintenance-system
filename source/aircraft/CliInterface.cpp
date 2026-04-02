#include <aircraft/CliInterface.h>
#include <iostream>
#include <iomanip>
#include <limits>
#include <chrono>
#include <sstream>

using namespace aircraft;

CliInterface::CliInterface(Aircraft& aircraft) : m_aircraft(aircraft) {}

void CliInterface::clearScreen() {
    // Cross-platform clear screen
    #ifdef _WIN32
        system("cls");
    #else
        system("clear");
    #endif
}

void CliInterface::waitForEnter() {
    std::cout << "\nPress Enter to continue...";
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    std::cin.get();
}

void CliInterface::printHeader(const std::string& title) {
    std::cout << "\n========================================\n";
    std::cout << "   " << title << "\n";
    std::cout << "========================================\n";
}

void CliInterface::printSeparator() {
    std::cout << "----------------------------------------\n";
}

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
        std::cout << "\nCurrent State: ";
        std::string state = m_aircraft.getCurrentState();
        if (state == "STANDBY") {
            std::cout << "\033[32m"; // Green
        } else if (state == "DIAGNOSTIC") {
            std::cout << "\033[33m"; // Yellow
        } else if (state == "MAINTENANCE") {
            std::cout << "\033[36m"; // Cyan
        } else if (state == "FAULT") {
            std::cout << "\033[31m"; // Red
        }
        std::cout << state << "\033[0m\n";
        
        printSeparator();
        
        std::cout << "\nPlease select an option:\n";
        std::cout << "  1. View Current State\n";
        std::cout << "  2. View Last Maintenance Time\n";
        std::cout << "  3. View Fault Codes\n";
        std::cout << "  4. View Warranty Status\n";
        std::cout << "  5. View All Information\n";
        std::cout << "  6. Exit\n";
        printSeparator();
        std::cout << "Enter choice (1-6): ";
        
        int choice;
        std::cin >> choice;
        
        if (std::cin.fail()) {
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::cout << "Invalid input. Please enter a number.\n";
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
                std::cout << "\nExiting application...\n";
                running = false;
                break;
            default:
                std::cout << "Invalid choice. Please select 1-6.\n";
                waitForEnter();
        }
    }
}

void CliInterface::displayCurrentState() {
    clearScreen();
    printHeader("CURRENT AIRCRAFT STATE");
    std::cout << "\nOperational State: " << m_aircraft.getCurrentState() << "\n";
    
    // Add state description
    std::string state = m_aircraft.getCurrentState();
    if (state == "STANDBY") {
        std::cout << "\nDescription: Aircraft is on standby, awaiting commands.\n";
        std::cout << "Available: Landed notification, diagnostic entry.\n";
    } else if (state == "DIAGNOSTIC") {
        std::cout << "\nDescription: Aircraft is running diagnostics.\n";
        std::cout << "Available: System checks, data transfer, schematic upload.\n";
    } else if (state == "MAINTENANCE") {
        std::cout << "\nDescription: Aircraft is in maintenance mode.\n";
        std::cout << "Available: Repairs, component replacement, system tests.\n";
    } else if (state == "FAULT") {
        std::cout << "\nDescription: Aircraft has detected a fault.\n";
        std::cout << "Action Required: Diagnostic check needed.\n";
    }
}

void CliInterface::displayLastMaintenance() {
    clearScreen();
    printHeader("LAST MAINTENANCE RECORD");
    
    MaintenanceInfo info = m_aircraft.getLastMaintenance();
    std::cout << "\nDate/Time: " << formatTimePoint(info.lastMaintenance) << "\n";
    std::cout << "Technician: " << info.technician << "\n";
    std::cout << "Notes: " << info.notes << "\n";
}

void CliInterface::displayFaultCodes() {
    clearScreen();
    printHeader("FAULT CODES");
    
    auto faults = m_aircraft.getFaultCodes();
    if (faults.empty()) {
        std::cout << "\nNo active fault codes.\n";
        std::cout << "All systems operational.\n";
    } else {
        std::cout << "\nActive Fault Codes:\n\n";
        for (const auto& fault : faults) {
            std::cout << "  [CODE " << fault.code << "]\n";
            std::cout << "    Description: " << fault.description << "\n";
            std::cout << "    Detected: " << formatTimePoint(fault.timestamp) << "\n\n";
        }
    }
}

void CliInterface::displayWarrantyStatus() {
    clearScreen();
    printHeader("WARRANTY STATUS");
    
    WarrantyInfo info = m_aircraft.getWarranty();
    std::cout << "\nStatus: ";
    if (info.isActive) {
        std::cout << "\033[32mACTIVE\033[0m\n";
        std::cout << "Expires: " << info.expiryDate << "\n";
        std::cout << "Provider: " << info.provider << "\n";
        
        // Calculate days remaining
        std::cout << "\nCoverage: Full parts and labor\n";
        std::cout << "Contact: warranty@aviationcorp.com\n";
    } else {
        std::cout << "\033[31mEXPIRED\033[0m\n";
        std::cout << "Expired on: " << info.expiryDate << "\n";
        std::cout << "\nPlease contact service center for renewal options.\n";
    }
}

void CliInterface::displayAllInfo() {
    clearScreen();
    printHeader("COMPLETE AIRCRAFT STATUS");
    
    // Current State
    std::cout << "\n[STATE]\n";
    std::cout << "  " << m_aircraft.getCurrentState() << "\n";
    
    // Maintenance
    std::cout << "\n[MAINTENANCE]\n";
    MaintenanceInfo maint = m_aircraft.getLastMaintenance();
    std::cout << "  Last Maintenance: " << formatTimePoint(maint.lastMaintenance) << "\n";
    std::cout << "  Technician: " << maint.technician << "\n";
    
    // Warranty
    std::cout << "\n[WARRANTY]\n";
    WarrantyInfo warranty = m_aircraft.getWarranty();
    std::cout << "  Status: " << (warranty.isActive ? "ACTIVE" : "EXPIRED") << "\n";
    std::cout << "  Expiry: " << warranty.expiryDate << "\n";
    std::cout << "  Provider: " << warranty.provider << "\n";
    
    // Fault Codes
    std::cout << "\n[FAULT CODES]\n";
    auto faults = m_aircraft.getFaultCodes();
    if (faults.empty()) {
        std::cout << "  None detected\n";
    } else {
        for (const auto& fault : faults) {
            std::cout << "  " << fault.code << ": " << fault.description << "\n";
        }
    }
    
    printSeparator();
}