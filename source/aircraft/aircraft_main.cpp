#include <aircraft/Aircraft.h>
#include <aircraft/CliInterface.h>
#include <aircraft/StateManager.h>

int main() {
  aircraft::Aircraft aircraft;
  aircraft.initialize();
  // Start connection to MMA on localhost port 8000
  aircraft.connectToMMA("127.0.0.1", 8000);

  StateManager stateManager;
  aircraft.setStateManager(&stateManager);
  aircraft.syncStateManagerToCurrentState();

  aircraft::CliInterface cli(aircraft);
  cli.showMainMenu();

  return 0;
}