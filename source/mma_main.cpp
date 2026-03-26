#include <iostream>
#include <mma/mma.h>

int main() {
  std::cout << "MMA Server Starting..." << std::endl;
  
  mma::MMA mma;
  mma.initialize();
  
  std::cout << "MMA Server Running" << std::endl;
  
  return 0;
}
