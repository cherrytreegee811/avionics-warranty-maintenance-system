#include <mma/mma.h>
#include <iostream>
#include <thread>

int main() {
  MMA mma;
  mma.initialize();
  mma.startServer(8000);

  std::cout << "MMA server running on port 8000. Press Enter to exit.\n";
  std::cin.get();
  return 0;
}
