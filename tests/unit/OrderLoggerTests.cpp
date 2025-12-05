#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "config/Config.h"
#include "logs/OrderLogger.h"

using namespace std::chrono_literals;
using ::testing::HasSubstr;

namespace fs = std::filesystem;

// ============================================================================
// Test Fixture
// ============================================================================

class OrderLoggerTest : public ::testing::Test {
 protected:
  fs::path temp_dir;
  fs::path test_file_path;

  void SetUp() override {
    auto timestamp =
        std::chrono::system_clock::now().time_since_epoch().count();
    temp_dir = fs::temp_directory_path() /
               std::format("order_logger_test_{}", timestamp);
    fs::create_directories(temp_dir);
    test_file_path = temp_dir / "orders.csv";
  }

  void TearDown() override {
    std::error_code ec;
    fs::remove_all(temp_dir, ec);
  }

  Config CreateTestConfig() {
    Config cfg;
    cfg.orders_log_path = test_file_path;
    return cfg;
  }

  std::string ReadFileContent() {
    std::ifstream file(test_file_path);
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
  }

  std::vector<std::string> ReadFileLines() {
    std::vector<std::string> lines;
    std::ifstream file(test_file_path);
    std::string line;
    while (std::getline(file, line)) {
      lines.push_back(line);
    }
    return lines;
  }
};

// ============================================================================
// Constructor Tests
// ============================================================================

TEST_F(OrderLoggerTest, Constructor_ValidPath_CreatesFile) {
  Config cfg = CreateTestConfig();

  OrderLogger logger(cfg);

  EXPECT_TRUE(fs::exists(test_file_path));
}

TEST_F(OrderLoggerTest, Constructor_CreatesDirectories) {
  Config cfg;
  cfg.orders_log_path = temp_dir / "subdir1" / "subdir2" / "orders.csv";

  OrderLogger logger(cfg);

  EXPECT_TRUE(fs::exists(cfg.orders_log_path));
}

TEST_F(OrderLoggerTest, Constructor_InvalidPath_Throws) {
  Config cfg;
#ifdef _WIN32
  cfg.orders_log_path = "Z:\\nonexistent\\path\\<>:\"|?*\\file.csv";
#else
  cfg.orders_log_path = "/nonexistent/path/that/does/not/exist/file.csv";
#endif

  EXPECT_THROW(OrderLogger logger(cfg), std::runtime_error);
}

// ============================================================================
// writeOrder - Side Tests
// ============================================================================

TEST_F(OrderLoggerTest, WriteOrder_BuyOrder_CorrectSideString) {
  Config cfg = CreateTestConfig();
  OrderLogger logger(cfg);

  auto result =
      logger.writeOrder(OrderSide::Buy, 100.0, 50.0, Status::Executed, "", 0.0);

  EXPECT_EQ(result, std::nullopt);
  std::string content = ReadFileContent();
  EXPECT_THAT(content, HasSubstr("Buy"));
}

TEST_F(OrderLoggerTest, WriteOrder_SellOrder_CorrectSideString) {
  Config cfg = CreateTestConfig();
  OrderLogger logger(cfg);

  auto result = logger.writeOrder(OrderSide::Sell, 100.0, 50.0,
                                  Status::Executed, "", 0.0);

  EXPECT_EQ(result, std::nullopt);
  std::string content = ReadFileContent();
  EXPECT_THAT(content, HasSubstr("Sell"));
}

// ============================================================================
// writeOrder - Status Tests
// ============================================================================

TEST_F(OrderLoggerTest, WriteOrder_ExecutedStatus_CorrectString) {
  Config cfg = CreateTestConfig();
  OrderLogger logger(cfg);

  logger.writeOrder(OrderSide::Buy, 100.0, 50.0, Status::Executed, "", 0.0);

  std::string content = ReadFileContent();
  EXPECT_THAT(content, HasSubstr("Executed"));
}

TEST_F(OrderLoggerTest, WriteOrder_RejectedStatus_CorrectString) {
  Config cfg = CreateTestConfig();
  OrderLogger logger(cfg);

  logger.writeOrder(OrderSide::Buy, 100.0, 50.0, Status::Rejected,
                    "Test rejection", 0.0);

  std::string content = ReadFileContent();
  EXPECT_THAT(content, HasSubstr("Rejected"));
}

TEST_F(OrderLoggerTest, WriteOrder_PendingStatus_CorrectString) {
  Config cfg = CreateTestConfig();
  OrderLogger logger(cfg);

  logger.writeOrder(OrderSide::Buy, 100.0, 50.0, Status::Pending, "", 0.0);

  std::string content = ReadFileContent();
  EXPECT_THAT(content, HasSubstr("Pending"));
}

// ============================================================================
// writeOrder - Error Text Tests
// ============================================================================

TEST_F(OrderLoggerTest, WriteOrder_WithErrorText_Included) {
  Config cfg = CreateTestConfig();
  OrderLogger logger(cfg);

  logger.writeOrder(OrderSide::Buy, 100.0, 50.0, Status::Rejected,
                    "Random rejection", 0.0);

  std::string content = ReadFileContent();
  EXPECT_THAT(content, HasSubstr("Random rejection"));
}

TEST_F(OrderLoggerTest, WriteOrder_EmptyErrorText_EmptyField) {
  Config cfg = CreateTestConfig();
  OrderLogger logger(cfg);

  auto result =
      logger.writeOrder(OrderSide::Buy, 100.0, 50.0, Status::Executed, "", 0.0);

  EXPECT_EQ(result, std::nullopt);
  // File should still be valid
  auto lines = ReadFileLines();
  EXPECT_EQ(lines.size(), 2);  // Header + 1 order
}

// ============================================================================
// writeOrder - Format Tests
// ============================================================================

TEST_F(OrderLoggerTest, WriteOrder_PriceFormat_3Decimals) {
  Config cfg = CreateTestConfig();
  OrderLogger logger(cfg);

  logger.writeOrder(OrderSide::Buy, 123.456789, 50.0, Status::Executed, "",
                    0.0);

  std::string content = ReadFileContent();
  EXPECT_THAT(content, HasSubstr("123.457"));  // Rounded to 3 decimals
}

TEST_F(OrderLoggerTest, WriteOrder_VolumeFormat_3Decimals) {
  Config cfg = CreateTestConfig();
  OrderLogger logger(cfg);

  logger.writeOrder(OrderSide::Buy, 100.0, 78.9012345, Status::Executed, "",
                    0.0);

  std::string content = ReadFileContent();
  EXPECT_THAT(content, HasSubstr("78.901"));  // Rounded to 3 decimals
}

TEST_F(OrderLoggerTest, WriteOrder_PnLFormat_3Decimals) {
  Config cfg = CreateTestConfig();
  OrderLogger logger(cfg);

  logger.writeOrder(OrderSide::Buy, 100.0, 50.0, Status::Executed, "",
                    -5000.123456);

  std::string content = ReadFileContent();
  EXPECT_THAT(content, HasSubstr("-5000.123"));  // Rounded to 3 decimals
}

// ============================================================================
// writeOrder - Return Value Tests
// ============================================================================

TEST_F(OrderLoggerTest, WriteOrder_ReturnsNulloptOnSuccess) {
  Config cfg = CreateTestConfig();
  OrderLogger logger(cfg);

  auto result =
      logger.writeOrder(OrderSide::Buy, 100.0, 50.0, Status::Executed, "", 0.0);

  EXPECT_EQ(result, std::nullopt);
}

// ============================================================================
// writeOrder - Multiple Orders Tests
// ============================================================================

TEST_F(OrderLoggerTest, WriteOrder_MultipleOrders_AllWritten) {
  Config cfg = CreateTestConfig();
  OrderLogger logger(cfg);

  logger.writeOrder(OrderSide::Buy, 100.0, 50.0, Status::Executed, "", -5000.0);
  logger.writeOrder(OrderSide::Sell, 101.0, 50.0, Status::Executed, "", 50.0);
  logger.writeOrder(OrderSide::Buy, 99.0, 25.0, Status::Rejected, "Error",
                    50.0);

  auto lines = ReadFileLines();
  EXPECT_EQ(lines.size(), 4);  // Header + 3 orders
}

TEST_F(OrderLoggerTest, WriteOrder_CSVFormat_CommaSeparated) {
  Config cfg = CreateTestConfig();
  OrderLogger logger(cfg);

  logger.writeOrder(OrderSide::Buy, 100.0, 50.0, Status::Executed, "", -5000.0);

  auto lines = ReadFileLines();
  ASSERT_GE(lines.size(), 2);
  // Count commas in data line (should have 5 commas for 6 fields)
  int comma_count = std::count(lines[1].begin(), lines[1].end(), ',');
  EXPECT_EQ(comma_count, 5);
}

TEST_F(OrderLoggerTest, WriteOrder_SequentialWrites_MaintainOrder) {
  Config cfg = CreateTestConfig();
  OrderLogger logger(cfg);

  for (int i = 0; i < 5; ++i) {
    logger.writeOrder(OrderSide::Buy, 100.0 + i, 50.0, Status::Executed, "",
                      static_cast<double>(-i * 100));
  }

  auto lines = ReadFileLines();
  EXPECT_EQ(lines.size(), 6);  // Header + 5 orders
}

// ============================================================================
// writeOrder - Edge Cases Tests
// ============================================================================

TEST_F(OrderLoggerTest, WriteOrder_ZeroPrice) {
  Config cfg = CreateTestConfig();
  OrderLogger logger(cfg);

  auto result =
      logger.writeOrder(OrderSide::Buy, 0.0, 50.0, Status::Executed, "", 0.0);

  EXPECT_EQ(result, std::nullopt);
  std::string content = ReadFileContent();
  EXPECT_THAT(content, HasSubstr("0.000"));
}

TEST_F(OrderLoggerTest, WriteOrder_NegativePnL) {
  Config cfg = CreateTestConfig();
  OrderLogger logger(cfg);

  logger.writeOrder(OrderSide::Buy, 100.0, 50.0, Status::Executed, "",
                    -10000.0);

  std::string content = ReadFileContent();
  EXPECT_THAT(content, HasSubstr("-10000.000"));
}

TEST_F(OrderLoggerTest, WriteOrder_PositivePnL) {
  Config cfg = CreateTestConfig();
  OrderLogger logger(cfg);

  logger.writeOrder(OrderSide::Sell, 100.0, 50.0, Status::Executed, "", 5000.0);

  std::string content = ReadFileContent();
  EXPECT_THAT(content, HasSubstr("5000.000"));
}
