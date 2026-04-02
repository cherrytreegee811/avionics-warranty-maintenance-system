#include <mma/mma.h>

int main() {
  MMA mma;
  mma.initialize();
  mma.startServer(8000);

  // Run the technician menu in the main thread
  mma.runMenu();

  // Clean shutdown
  mma.stopServer();
  return 0;
}
