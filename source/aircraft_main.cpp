#include <aircraft/aircraft.h>

#include <iostream>

int main() {
  std::cout << "Aircraft Client Starting..." << std::endl;

  aircraft::Aircraft aircraft;
  aircraft.initialize();

  std::cout << "Aircraft Client Running" << std::endl;

  return 0;
}
