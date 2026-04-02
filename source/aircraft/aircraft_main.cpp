#include <aircraft/Aircraft.h>
#include <aircraft/CliInterface.h>
#include <aircraft/StandbyState.h>
#include <aircraft/StateManager.h>

#include <iostream>
#include <memory>

int main() {
    aircraft::Aircraft aircraft;
    aircraft.initialize();

    StateManager stateManager;
    stateManager.SetState(std::make_unique<StandbyState>(aircraft, stateManager));

    // Create and show CLI interface
    aircraft::CliInterface cli(aircraft);
    cli.showMainMenu();

    return 0;
}