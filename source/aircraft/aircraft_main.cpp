
#include <aircraft/Aircraft.h>
#include <aircraft/StateManager.h>
#include <aircraft/StandbyState.h>
#include <iostream>

int main() {
    aircraft::Aircraft aircraft;
    aircraft.initialize();

    StateManager stateManager;
    stateManager.SetState(std::make_unique<StandbyState>(aircraft, stateManager));


    bool running = true;
    while (running) {
        stateManager.Update();
    }

    return 0;
}
