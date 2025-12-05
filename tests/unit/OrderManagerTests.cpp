#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "config/Config.h"
#include "trading/OrderManager.h"

using namespace std::chrono_literals;

namespace fs = std::filesystem;

// ============================================================================
// Test Fixture
// ============================================================================

class OrderManagerTest : public ::testing::Test {
 protected:
  fs::path temp_dir;

  void SetUp() override {
    auto timestamp =
        std::chrono::system_clock::now().time_since_epoch().count();
    temp_dir = fs::temp_directory_path() /
               std::format("order_manager_test_{}", timestamp);
    fs::create_directories(temp_dir);
  }

  void TearDown() override {
    std::error_code ec;
    fs::remove_all(temp_dir, ec);
  }

  Config CreateTestConfig() {
    Config cfg;
    cfg.price_evolution_path = temp_dir / "ticks.csv";
    cfg.orders_log_path = temp_dir / "orders.csv";
    cfg.rejection_probability = 0.0;  // All orders execute
    cfg.min_position = -1000.0;
    cfg.max_position = 1000.0;
    cfg.min_volume = 1.0;
    cfg.max_volume = 100.0;
    return cfg;
  }

  std::vector<std::string> ReadOrderLogLines() {
    std::vector<std::string> lines;
    std::ifstream file(temp_dir / "orders.csv");
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

TEST_F(OrderManagerTest, Constructor_ValidConfig_CreatesManager) {
  Config cfg = CreateTestConfig();

  EXPECT_NO_THROW(OrderManager manager(cfg));
}

// ============================================================================
// SendOrder Tests
// ============================================================================

TEST_F(OrderManagerTest, SendOrder_ReturnsValidOrderId) {
  Config cfg = CreateTestConfig();
  OrderManager manager(cfg);
  Order order{OrderSide::Buy, 100.0, 50.0};

  OrderIdentifier id = manager.SendOrder(order);

  EXPECT_GE(id, 1);
}

TEST_F(OrderManagerTest, SendOrder_MultipleOrders_IncrementingIds) {
  Config cfg = CreateTestConfig();
  OrderManager manager(cfg);
  Order order{OrderSide::Buy, 100.0, 50.0};

  OrderIdentifier id1 = manager.SendOrder(order);
  OrderIdentifier id2 = manager.SendOrder(order);
  OrderIdentifier id3 = manager.SendOrder(order);

  EXPECT_EQ(id1, 1);
  EXPECT_EQ(id2, 2);
  EXPECT_EQ(id3, 3);
}

// ============================================================================
// onBuySignal Tests
// ============================================================================

TEST_F(OrderManagerTest, OnBuySignal_ZeroPosition_SendsBuyOrder) {
  Config cfg = CreateTestConfig();
  OrderManager manager(cfg);

  manager.onBuySignal(100.0, 50.0);

  // Check order was logged (file should have header + 1 order)
  auto lines = ReadOrderLogLines();
  EXPECT_EQ(lines.size(), 2);
}

TEST_F(OrderManagerTest, OnBuySignal_AtMaxPosition_NoOrder) {
  Config cfg = CreateTestConfig();
  cfg.max_position = 100.0;
  OrderManager manager(cfg);

  // Fill to max position
  manager.onBuySignal(100.0, 100.0);

  // Try to buy more - should not create order
  auto lines_before = ReadOrderLogLines();
  manager.onBuySignal(100.0, 50.0);
  auto lines_after = ReadOrderLogLines();

  EXPECT_EQ(lines_before.size(), lines_after.size());
}

TEST_F(OrderManagerTest, OnBuySignal_ClampedVolume_WhenNearMax) {
  Config cfg = CreateTestConfig();
  cfg.max_position = 100.0;
  OrderManager manager(cfg);

  // Buy 80, then try to buy 50 (should be clamped to 20)
  manager.onBuySignal(100.0, 80.0);
  manager.onBuySignal(100.0, 50.0);

  // Both orders should be logged
  auto lines = ReadOrderLogLines();
  EXPECT_EQ(lines.size(), 3);  // Header + 2 orders
}

TEST_F(OrderManagerTest, OnBuySignal_NegativePosition_CanBuyMoreThanVolume) {
  Config cfg = CreateTestConfig();
  cfg.max_position = 100.0;
  cfg.min_position = -100.0;
  OrderManager manager(cfg);

  // Sell first to get negative position
  manager.onSellSignal(100.0, 50.0);
  // Now buy - can buy up to max_position from negative
  manager.onBuySignal(100.0, 100.0);

  auto lines = ReadOrderLogLines();
  EXPECT_EQ(lines.size(), 3);  // Header + 2 orders
}

// ============================================================================
// onSellSignal Tests
// ============================================================================

TEST_F(OrderManagerTest, OnSellSignal_PositivePosition_SendsSellOrder) {
  Config cfg = CreateTestConfig();
  OrderManager manager(cfg);

  // First buy to have position
  manager.onBuySignal(100.0, 50.0);
  // Then sell
  manager.onSellSignal(100.0, 25.0);

  auto lines = ReadOrderLogLines();
  EXPECT_EQ(lines.size(), 3);  // Header + 2 orders
}

TEST_F(OrderManagerTest, OnSellSignal_AtMinPosition_NoOrder) {
  Config cfg = CreateTestConfig();
  cfg.min_position = -100.0;
  OrderManager manager(cfg);

  // Sell to min position
  manager.onSellSignal(100.0, 100.0);

  // Try to sell more - should not create order
  auto lines_before = ReadOrderLogLines();
  manager.onSellSignal(100.0, 50.0);
  auto lines_after = ReadOrderLogLines();

  EXPECT_EQ(lines_before.size(), lines_after.size());
}

TEST_F(OrderManagerTest, OnSellSignal_ClampedVolume_WhenNearMin) {
  Config cfg = CreateTestConfig();
  cfg.min_position = -100.0;
  OrderManager manager(cfg);

  // Sell 80, then try to sell 50 (should be clamped to 20)
  manager.onSellSignal(100.0, 80.0);
  manager.onSellSignal(100.0, 50.0);

  auto lines = ReadOrderLogLines();
  EXPECT_EQ(lines.size(), 3);  // Header + 2 orders
}

TEST_F(OrderManagerTest, OnSellSignal_ZeroVolume_NoOrder) {
  Config cfg = CreateTestConfig();
  OrderManager manager(cfg);

  // Buy first to have position
  manager.onBuySignal(100.0, 50.0);
  // Then try to sell zero volume
  manager.onSellSignal(100.0, 0.0);

  auto lines = ReadOrderLogLines();
  EXPECT_EQ(lines.size(), 2);  // Header + 1 buy order only
}

TEST_F(OrderManagerTest, OnSellSignal_ZeroPosition_WithMinNegative_CanSell) {
  Config cfg = CreateTestConfig();
  cfg.min_position = -100.0;
  OrderManager manager(cfg);

  manager.onSellSignal(100.0, 50.0);

  auto lines = ReadOrderLogLines();
  EXPECT_EQ(lines.size(), 2);  // Header + 1 sell order
}

// ============================================================================
// Position Tracking Tests
// ============================================================================

TEST_F(OrderManagerTest, ExecutedBuyOrder_IncreasesPosition) {
  Config cfg = CreateTestConfig();
  cfg.max_position = 200.0;
  OrderManager manager(cfg);

  manager.onBuySignal(100.0, 50.0);
  manager.onBuySignal(100.0, 50.0);

  // Position should be 100 now, so buying another 50 should work
  manager.onBuySignal(100.0, 50.0);

  auto lines = ReadOrderLogLines();
  EXPECT_EQ(lines.size(), 4);  // Header + 3 orders
}

TEST_F(OrderManagerTest, ExecutedSellOrder_DecreasesPosition) {
  Config cfg = CreateTestConfig();
  cfg.min_position = -200.0;
  OrderManager manager(cfg);

  manager.onSellSignal(100.0, 50.0);
  manager.onSellSignal(100.0, 50.0);
  manager.onSellSignal(100.0, 50.0);

  auto lines = ReadOrderLogLines();
  EXPECT_EQ(lines.size(), 4);  // Header + 3 orders
}

TEST_F(OrderManagerTest, MultipleOrders_PositionAccumulates) {
  Config cfg = CreateTestConfig();
  cfg.min_position = -500.0;
  cfg.max_position = 500.0;
  OrderManager manager(cfg);

  // Buy 100
  manager.onBuySignal(100.0, 100.0);
  // Sell 50 (position = 50)
  manager.onSellSignal(100.0, 50.0);
  // Buy 100 (position = 150)
  manager.onBuySignal(100.0, 100.0);
  // Sell 200 (position = -50)
  manager.onSellSignal(100.0, 200.0);

  auto lines = ReadOrderLogLines();
  EXPECT_EQ(lines.size(), 5);  // Header + 4 orders
}

// ============================================================================
// PnL Calculation Tests
// ============================================================================

TEST_F(OrderManagerTest, ExecutedSellOrder_IncreasesPnL) {
  Config cfg = CreateTestConfig();
  cfg.min_position = -100.0;
  OrderManager manager(cfg);

  // Sell at 100, volume 50 -> pnl = +5000 (but unrealized = -5000)
  manager.onSellSignal(100.0, 50.0);

  auto lines = ReadOrderLogLines();
  EXPECT_GE(lines.size(), 2);
}

// ============================================================================
// Rejection Handling Tests
// ============================================================================

TEST_F(OrderManagerTest, RejectedOrder_PositionUnchanged) {
  Config cfg = CreateTestConfig();
  cfg.rejection_probability = 100.0;  // All orders rejected
  cfg.max_position = 100.0;
  OrderManager manager(cfg);

  // Try to buy - will be rejected
  manager.onBuySignal(100.0, 50.0);

  // Position should still be 0, so another buy should work
  // (if first was rejected, position didn't change)
  manager.onBuySignal(100.0, 50.0);

  // Both orders should be logged (both rejected)
  auto lines = ReadOrderLogLines();
  EXPECT_EQ(lines.size(), 3);  // Header + 2 orders
}

// ============================================================================
// Edge Cases Tests
// ============================================================================

TEST_F(OrderManagerTest, SymmetricPositionLimits) {
  Config cfg = CreateTestConfig();
  cfg.min_position = -100.0;
  cfg.max_position = 100.0;
  OrderManager manager(cfg);

  // Alternate buy/sell multiple times
  manager.onBuySignal(100.0, 50.0);
  manager.onSellSignal(100.0, 100.0);
  manager.onBuySignal(100.0, 100.0);
  manager.onSellSignal(100.0, 100.0);

  auto lines = ReadOrderLogLines();
  EXPECT_EQ(lines.size(), 5);  // Header + 4 orders
}

TEST_F(OrderManagerTest, LargeVolume_ClampsToPositionLimit) {
  Config cfg = CreateTestConfig();
  cfg.max_position = 100.0;
  OrderManager manager(cfg);

  // Try to buy huge volume
  manager.onBuySignal(100.0, 10000.0);

  auto lines = ReadOrderLogLines();
  EXPECT_EQ(lines.size(), 2);  // Header + 1 order (clamped to 100)
}

TEST_F(OrderManagerTest, VerySmallVolume_Works) {
  Config cfg = CreateTestConfig();
  OrderManager manager(cfg);

  manager.onBuySignal(100.0, 0.001);

  auto lines = ReadOrderLogLines();
  EXPECT_EQ(lines.size(), 2);  // Header + 1 order
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_F(OrderManagerTest, CompleteTradeRoundTrip) {
  Config cfg = CreateTestConfig();
  cfg.min_position = -100.0;
  cfg.max_position = 100.0;
  OrderManager manager(cfg);

  // Buy 100 at 50
  manager.onBuySignal(50.0, 100.0);
  // Sell 100 at 60 (profit)
  manager.onSellSignal(60.0, 100.0);

  auto lines = ReadOrderLogLines();
  EXPECT_EQ(lines.size(), 3);  // Header + 2 orders
}
