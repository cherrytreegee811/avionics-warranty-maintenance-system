#include <mma/mma.h>

#include <iostream>

int main() {
  std::cout << "MMA Server Starting..." << std::endl;

  mma::MMA mma;
  mma.initialize();

  std::cout << "MMA Server Running" << std::endl;

  return 0;
}
