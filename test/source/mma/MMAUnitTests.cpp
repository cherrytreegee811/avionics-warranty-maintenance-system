#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <common/WarrantyData.h>
#include <doctest/doctest.h>
#include <mma/WarrantyManager.h>

#include <cstdio>
#include <fstream>

// ============================================================================
// REQ-SRV-008: The server should be able to view if a warranty is active or expired.
// ============================================================================

TEST_CASE("REQ-SRV-008: WarrantyManager stores and retrieves warranty info") {
  const std::string testFile = "test_warranty.csv";
  std::remove(testFile.c_str());

  mma::WarrantyManager mgr(testFile);
  common::WarrantyInfo info{true, "2028-12-31", "TestProvider"};
  mgr.setWarranty(1001, info);

  auto retrieved = mgr.getWarranty(1001);
  REQUIRE(retrieved.has_value());
  CHECK(retrieved->isActive == true);
  CHECK(retrieved->expiryDate == "2028-12-31");
  CHECK(retrieved->provider == "TestProvider");

  // Test persistence: create new manager with same file
  mma::WarrantyManager mgr2(testFile);
  mgr2.load();
  auto retrieved2 = mgr2.getWarranty(1001);
  CHECK(retrieved2.has_value());
  CHECK(retrieved2->expiryDate == "2028-12-31");

  std::remove(testFile.c_str());
}

TEST_CASE("REQ-SRV-008: WarrantyManager retrieves and reports expired warranty") {
  const std::string testFile = "expired_warranty.csv";
  std::remove(testFile.c_str());

  // Store an expired warranty (isActive = false)
  mma::WarrantyManager mgr(testFile);
  common::WarrantyInfo expiredInfo{false, "2020-01-01", "OldProvider"};
  mgr.setWarranty(9999, expiredInfo);

  // Retrieve and verify it's reported as expired
  auto retrieved = mgr.getWarranty(9999);
  REQUIRE(retrieved.has_value());
  CHECK(retrieved->isActive == false);
  CHECK(retrieved->expiryDate == "2020-01-01");
  CHECK(retrieved->provider == "OldProvider");

  std::remove(testFile.c_str());
}

TEST_CASE("REQ-SRV-008: WarrantyManager returns nullopt for unknown ID") {
  mma::WarrantyManager mgr("unused.csv");
  auto retrieved = mgr.getWarranty(9999);
  CHECK(!retrieved.has_value());
}

TEST_CASE("REQ-SRV-008: WarrantyManager handles empty file gracefully") {
  const std::string testFile = "empty_warranty.csv";
  std::remove(testFile.c_str());

  // Create empty file
  std::ofstream empty(testFile);
  empty.close();

  mma::WarrantyManager mgr(testFile);
  bool loaded = mgr.load();
  CHECK(loaded == true);
  auto retrieved = mgr.getWarranty(123);
  CHECK(!retrieved.has_value());

  std::remove(testFile.c_str());
}

TEST_CASE("REQ-SRV-008: WarrantyManager saves data in correct CSV format") {
  const std::string testFile = "format_warranty.csv";
  std::remove(testFile.c_str());

  mma::WarrantyManager mgr(testFile);
  common::WarrantyInfo info{false, "2025-01-01", "ProviderX"};
  mgr.setWarranty(42, info);

  // Read the file directly
  std::ifstream file(testFile);
  REQUIRE(file.is_open());
  std::string line;
  std::getline(file, line);
  CHECK(line == "42,0,2025-01-01,ProviderX");
  file.close();

  std::remove(testFile.c_str());
}

TEST_CASE("REQ-SRV-008: WarrantyManager load returns false when file is missing") {
  const std::string missingFile = "missing_warranty_file_should_not_exist.csv";
  std::remove(missingFile.c_str());

  mma::WarrantyManager mgr(missingFile);
  CHECK(mgr.load() == false);
  CHECK(!mgr.getWarranty(1).has_value());
}

TEST_CASE("REQ-SRV-008: WarrantyManager load skips malformed and empty CSV lines") {
  const std::string testFile = "mixed_warranty.csv";
  std::remove(testFile.c_str());

  {
    std::ofstream out(testFile);
    REQUIRE(out.is_open());
    out << "\n";
    out << "100,1,2030-12-31,ProviderA\n";
    out << "400,1\n";  // malformed/incomplete line
    out << "200,0,2020-01-01,ProviderB\n";
  }

  mma::WarrantyManager mgr(testFile);
  CHECK(mgr.load() == true);

  auto first = mgr.getWarranty(100);
  REQUIRE(first.has_value());
  CHECK(first->isActive == true);
  CHECK(first->expiryDate == "2030-12-31");
  CHECK(first->provider == "ProviderA");

  auto second = mgr.getWarranty(200);
  REQUIRE(second.has_value());
  CHECK(second->isActive == false);
  CHECK(second->expiryDate == "2020-01-01");
  CHECK(second->provider == "ProviderB");

  CHECK(!mgr.getWarranty(400).has_value());

  std::remove(testFile.c_str());
}

TEST_CASE("REQ-SRV-008: WarrantyManager save returns false when storage path is invalid") {
  // Using a directory path as output file should fail to open for writing.
  mma::WarrantyManager mgr(".");
  common::WarrantyInfo info{true, "2031-01-01", "ProviderInvalidPath"};
  mgr.setWarranty(77, info);

  CHECK(mgr.save() == false);
}