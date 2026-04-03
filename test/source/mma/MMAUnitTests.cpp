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

  WarrantyManager mgr(testFile);
  common::WarrantyInfo info{true, "2028-12-31", "TestProvider"};
  mgr.setWarranty(1001, info);

  auto retrieved = mgr.getWarranty(1001);
  REQUIRE(retrieved.has_value());
  CHECK(retrieved->isActive == true);
  CHECK(retrieved->expiryDate == "2028-12-31");
  CHECK(retrieved->provider == "TestProvider");

  // Test persistence: create new manager with same file
  WarrantyManager mgr2(testFile);
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
  WarrantyManager mgr(testFile);
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
  WarrantyManager mgr("unused.csv");
  auto retrieved = mgr.getWarranty(9999);
  CHECK(!retrieved.has_value());
}

TEST_CASE("REQ-SRV-008: WarrantyManager handles empty file gracefully") {
  const std::string testFile = "empty_warranty.csv";
  std::remove(testFile.c_str());

  // Create empty file
  std::ofstream empty(testFile);
  empty.close();

  WarrantyManager mgr(testFile);
  bool loaded = mgr.load();
  CHECK(loaded == true);
  auto retrieved = mgr.getWarranty(123);
  CHECK(!retrieved.has_value());

  std::remove(testFile.c_str());
}

TEST_CASE("REQ-SRV-008: WarrantyManager saves data in correct CSV format") {
  const std::string testFile = "format_warranty.csv";
  std::remove(testFile.c_str());

  WarrantyManager mgr(testFile);
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