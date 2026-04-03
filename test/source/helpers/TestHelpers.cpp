#include <helpers/TestHelpers.h>

#include <fstream>
#include <regex>

namespace test_helpers {

  bool logContains(const std::string& filename, const std::string& pattern) {
    std::ifstream file(filename);
    if (!file.is_open()) return false;
    std::regex re(pattern);
    std::string line;
    while (std::getline(file, line)) {
      if (std::regex_search(line, re)) return true;
    }
    return false;
  }

  bool logFileNonEmpty(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    return file.is_open() && file.tellg() > 0;
  }

}  // namespace test_helpers