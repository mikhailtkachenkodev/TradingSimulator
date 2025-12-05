#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "config/Config.h"
#include "trading/EmaTradingBot.h"

using namespace std::chrono_literals;
using ::testing::HasSubstr;

namespace fs = std::filesystem;

// ============================================================================
// Test Fixture
// ============================================================================

class EmaTradingBotTest : public ::testing::Test {
 protected:
  fs::path temp_dir;

  void SetUp() override {
    auto timestamp =
        std::chrono::system_clock::now().time_since_epoch().count();
    temp_dir = fs::temp_directory_path() /
               std::format("ema_trading_bot_test_{}", timestamp);
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
    cfg.fast_ema = 100ms;
    cfg.slow_ema = 500ms;
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

  int CountBuyOrders() {
    auto lines = ReadOrderLogLines();
    int count = 0;
    for (const auto& line : lines) {
      if (line.find("Buy") != std::string::npos) {
        count++;
      }
    }
    return count;
  }

  int CountSellOrders() {
    auto lines = ReadOrderLogLines();
    int count = 0;
    for (const auto& line : lines) {
      if (line.find("Sell") != std::string::npos) {
        count++;
      }
    }
    return count;
  }
};

// ============================================================================
// Constructor Tests
// ============================================================================

TEST_F(EmaTradingBotTest, Constructor_ValidConfig_CreatesBot) {
  Config cfg = CreateTestConfig();

  EXPECT_NO_THROW(EmaTradingBot bot(cfg));
}

// ============================================================================
// onTick - Crossover Signal Tests
// ============================================================================

TEST_F(EmaTradingBotTest, OnTick_FastCrossesAboveSlow_BuySignal) {
  Config cfg = CreateTestConfig();
  cfg.fast_ema = 10ms;    // Very fast
  cfg.slow_ema = 1000ms;  // Very slow
  EmaTradingBot bot(cfg);

  // Initial ticks to establish state (higher_ema_ becomes Slow)
  bot.onTick({0ns, 100.0, 50.0});
  bot.onTick({50ms, 95.0, 50.0});  // Price drops, fast follows faster than slow

  // Now rapid price increase - fast EMA will catch up faster
  for (int i = 1; i <= 20; ++i) {
    bot.onTick({std::chrono::milliseconds(50 + i * 10), 100.0 + i * 5.0, 50.0});
  }

  // Should have generated buy signals when fast crossed above slow
  int buy_count = CountBuyOrders();
  EXPECT_GE(buy_count, 1);
}

TEST_F(EmaTradingBotTest, OnTick_FastCrossesBelowSlow_SellSignal) {
  Config cfg = CreateTestConfig();
  cfg.fast_ema = 10ms;
  cfg.slow_ema = 1000ms;
  EmaTradingBot bot(cfg);

  // First establish Fast > Slow state
  bot.onTick({0ns, 100.0, 50.0});
  for (int i = 1; i <= 20; ++i) {
    bot.onTick({std::chrono::milliseconds(i * 10), 100.0 + i * 5.0, 50.0});
  }

  // Now rapid price decrease
  for (int i = 1; i <= 20; ++i) {
    bot.onTick(
        {std::chrono::milliseconds(200 + i * 10), 200.0 - i * 10.0, 50.0});
  }

  // Should have generated sell signals
  int sell_count = CountSellOrders();
  EXPECT_GE(sell_count, 1);
}

// ============================================================================
// onTick - No Duplicate Signals Tests
// ============================================================================

TEST_F(EmaTradingBotTest, OnTick_FastAboveSlow_NoBuyIfAlreadyFast) {
  Config cfg = CreateTestConfig();
  cfg.fast_ema = 10ms;
  cfg.slow_ema = 100ms;
  EmaTradingBot bot(cfg);

  // Establish Fast > Slow state
  bot.onTick({0ns, 100.0, 50.0});
  // Rapid increase to get Fast > Slow
  for (int i = 1; i <= 10; ++i) {
    bot.onTick({std::chrono::milliseconds(i * 10), 100.0 + i * 10.0, 50.0});
  }

  int buy_count_before = CountBuyOrders();

  // Continue increasing price (Fast should stay > Slow)
  for (int i = 11; i <= 20; ++i) {
    bot.onTick({std::chrono::milliseconds(i * 10), 100.0 + i * 10.0, 50.0});
  }

  int buy_count_after = CountBuyOrders();

  // Should not generate additional buy signals while Fast > Slow
  // (only generates on transition from Slow to Fast)
  EXPECT_EQ(buy_count_before, buy_count_after);
}

// ============================================================================
// onTick - Market Trend Tests
// ============================================================================

TEST_F(EmaTradingBotTest, OnTick_RisingMarket_EventuallyBuy) {
  Config cfg = CreateTestConfig();
  cfg.fast_ema = 50ms;
  cfg.slow_ema = 200ms;
  EmaTradingBot bot(cfg);

  // Steadily rising market
  for (int i = 0; i < 100; ++i) {
    bot.onTick({std::chrono::milliseconds(i * 20), 100.0 + i * 0.5, 50.0});
  }

  // In a rising market, fast EMA leads slow EMA
  // Should generate at least one buy signal after initial crossover
  int buy_count = CountBuyOrders();
  EXPECT_GE(buy_count, 1);
}

TEST_F(EmaTradingBotTest, OnTick_FallingMarket_EventuallySell) {
  Config cfg = CreateTestConfig();
  cfg.fast_ema = 50ms;
  cfg.slow_ema = 200ms;
  EmaTradingBot bot(cfg);

  // First rise to establish Fast state
  for (int i = 0; i < 50; ++i) {
    bot.onTick({std::chrono::milliseconds(i * 20), 100.0 + i * 1.0, 50.0});
  }

  // Then falling market
  for (int i = 50; i < 150; ++i) {
    bot.onTick(
        {std::chrono::milliseconds(i * 20), 150.0 - (i - 50) * 1.0, 50.0});
  }

  int sell_count = CountSellOrders();
  EXPECT_GE(sell_count, 1);
}

// ============================================================================
// onTick - Volatile Market Tests
// ============================================================================

TEST_F(EmaTradingBotTest, OnTick_VolatileMarket_MultipleSignals) {
  Config cfg = CreateTestConfig();
  cfg.fast_ema = 20ms;
  cfg.slow_ema = 100ms;
  EmaTradingBot bot(cfg);

  // Oscillating price
  for (int i = 0; i < 200; ++i) {
    double price = 100.0 + 20.0 * std::sin(i * 0.1);
    bot.onTick({std::chrono::milliseconds(i * 10), price, 50.0});
  }

  // Volatile market should generate multiple crossovers
  int buy_count = CountBuyOrders();
  int sell_count = CountSellOrders();
  int total_orders = buy_count + sell_count;

  // Should have multiple signals in volatile market
  EXPECT_GE(total_orders, 2);
}

// ============================================================================
// onTick - EMA Configuration Tests
// ============================================================================

TEST_F(EmaTradingBotTest, OnTick_VeryFastEMA_QuickResponse) {
  Config cfg = CreateTestConfig();
  cfg.fast_ema = 1ms;     // Extremely fast
  cfg.slow_ema = 1000ms;  // Very slow
  EmaTradingBot bot(cfg);

  bot.onTick({0ns, 100.0, 50.0});
  bot.onTick({10ms, 90.0, 50.0});   // Establish Slow state
  bot.onTick({20ms, 150.0, 50.0});  // Big jump - fast EMA responds immediately

  // Fast EMA should quickly catch up to new price
  int buy_count = CountBuyOrders();
  EXPECT_GE(buy_count, 1);
}

TEST_F(EmaTradingBotTest, OnTick_VerySlowEMA_SlowResponse) {
  Config cfg = CreateTestConfig();
  cfg.fast_ema = 100ms;
  cfg.slow_ema = 10s;  // Very slow
  EmaTradingBot bot(cfg);

  // Multiple ticks, slow EMA barely moves
  for (int i = 0; i < 50; ++i) {
    bot.onTick({std::chrono::milliseconds(i * 100), 100.0 + i, 50.0});
  }

  // With very slow EMA, crossovers take longer
  auto lines = ReadOrderLogLines();
  // At least header should exist
  EXPECT_GE(lines.size(), 1);
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_F(EmaTradingBotTest, Integration_FullCycle_BuyThenSell) {
  Config cfg = CreateTestConfig();
  cfg.fast_ema = 20ms;
  cfg.slow_ema = 80ms;
  EmaTradingBot bot(cfg);

  // Phase 1: Initialize
  bot.onTick({0ns, 100.0, 50.0});

  // Phase 2: Price drops, slow > fast
  for (int i = 1; i <= 10; ++i) {
    bot.onTick({std::chrono::milliseconds(i * 10), 100.0 - i * 2.0, 50.0});
  }

  // Phase 3: Price rises, triggers buy when fast crosses above slow
  for (int i = 11; i <= 40; ++i) {
    bot.onTick(
        {std::chrono::milliseconds(i * 10), 80.0 + (i - 10) * 3.0, 50.0});
  }

  // Phase 4: Price falls, triggers sell when fast crosses below slow
  for (int i = 41; i <= 70; ++i) {
    bot.onTick(
        {std::chrono::milliseconds(i * 10), 170.0 - (i - 40) * 3.0, 50.0});
  }

  int buy_count = CountBuyOrders();
  int sell_count = CountSellOrders();

  // Should have at least one buy and one sell
  EXPECT_GE(buy_count, 1);
  EXPECT_GE(sell_count, 1);
}

TEST_F(EmaTradingBotTest, Integration_ManyTicks_NoOverflow) {
  Config cfg = CreateTestConfig();
  cfg.fast_ema = 100ms;
  cfg.slow_ema = 500ms;
  EmaTradingBot bot(cfg);

  // Many ticks to ensure no overflow or memory issues
  for (int i = 0; i < 1000; ++i) {
    double price = 100.0 + 10.0 * std::sin(i * 0.01);
    bot.onTick({std::chrono::milliseconds(i * 50), price, 50.0});
  }

  // Just verify it completes without crashing
  auto lines = ReadOrderLogLines();
  EXPECT_GE(lines.size(), 1);
}

// ============================================================================
// Edge Case Tests
// ============================================================================

TEST_F(EmaTradingBotTest, OnTick_ZeroPrice) {
  Config cfg = CreateTestConfig();
  EmaTradingBot bot(cfg);

  // Zero price should be handled
  EXPECT_NO_THROW(bot.onTick({0ns, 0.0, 50.0}));
  EXPECT_NO_THROW(bot.onTick({100ms, 0.0, 50.0}));
}

TEST_F(EmaTradingBotTest, OnTick_VeryHighPrice) {
  Config cfg = CreateTestConfig();
  EmaTradingBot bot(cfg);

  bot.onTick({0ns, 1e10, 50.0});
  bot.onTick({100ms, 1e10 + 1e9, 50.0});

  // Should handle without overflow
  auto lines = ReadOrderLogLines();
  EXPECT_GE(lines.size(), 1);
}

TEST_F(EmaTradingBotTest, OnTick_ZeroVolume) {
  Config cfg = CreateTestConfig();
  EmaTradingBot bot(cfg);

  // Zero volume ticks
  bot.onTick({0ns, 100.0, 0.0});
  bot.onTick({100ms, 110.0, 0.0});

  // Should handle but maybe not generate orders (0 volume)
  EXPECT_NO_THROW(bot.onTick({200ms, 90.0, 0.0}));
}
