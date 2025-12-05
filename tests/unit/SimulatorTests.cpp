#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

#include "config/Config.h"
#include "simulation/Simulator.h"

using namespace std::chrono_literals;
using ::testing::HasSubstr;

namespace fs = std::filesystem;

// ============================================================================
// Test Fixture
// ============================================================================

class SimulatorTest : public ::testing::Test {
 protected:
  fs::path temp_dir;

  void SetUp() override {
    auto timestamp =
        std::chrono::system_clock::now().time_since_epoch().count();
    temp_dir =
        fs::temp_directory_path() / std::format("simulator_test_{}", timestamp);
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
    cfg.rejection_probability = 0.0;
    cfg.initial_price = 100.0;
    cfg.average_trend_value = 0.05;
    cfg.price_variation = 0.10;
    cfg.time_horizon = 24h;
    cfg.min_diff_time = 100ms;
    cfg.max_diff_time = 200ms;
    cfg.fast_ema = 1s;
    cfg.slow_ema = 5s;
    cfg.min_volume = 10.0;
    cfg.max_volume = 100.0;
    cfg.min_position = -1000.0;
    cfg.max_position = 1000.0;
    cfg.steps_count = 10;  // Small for fast tests
    return cfg;
  }

  std::vector<std::string> ReadTickLogLines() {
    std::vector<std::string> lines;
    std::ifstream file(temp_dir / "ticks.csv");
    std::string line;
    while (std::getline(file, line)) {
      lines.push_back(line);
    }
    return lines;
  }

  struct ParsedTick {
    std::string time;
    double price;
    double volume;
  };

  std::vector<ParsedTick> ParseTickLog() {
    std::vector<ParsedTick> ticks;
    auto lines = ReadTickLogLines();
    // Skip header
    for (size_t i = 1; i < lines.size(); ++i) {
      ParsedTick tick;
      std::stringstream ss(lines[i]);
      std::string token;
      std::getline(ss, tick.time, ',');
      std::getline(ss, token, ',');
      tick.price = std::stod(token);
      std::getline(ss, token, ',');
      tick.volume = std::stod(token);
      ticks.push_back(tick);
    }
    return ticks;
  }
};

// ============================================================================
// Constructor Tests
// ============================================================================

TEST_F(SimulatorTest, Constructor_ValidConfig_CreatesSimulator) {
  Config cfg = CreateTestConfig();

  EXPECT_NO_THROW(Simulator sim(cfg));
}

// ============================================================================
// Run Tests - Basic Execution
// ============================================================================

TEST_F(SimulatorTest, Run_ExecutesConfiguredSteps) {
  Config cfg = CreateTestConfig();
  cfg.steps_count = 50;
  Simulator sim(cfg);

  sim.Run();

  auto lines = ReadTickLogLines();
  EXPECT_EQ(lines.size(), 51);  // Header + 50 ticks
}

TEST_F(SimulatorTest, Run_SingleStep_OneIteration) {
  Config cfg = CreateTestConfig();
  cfg.steps_count = 1;
  Simulator sim(cfg);

  sim.Run();

  auto lines = ReadTickLogLines();
  EXPECT_EQ(lines.size(), 2);  // Header + 1 tick
}

// ============================================================================
// Run Tests - Price Generation
// ============================================================================

TEST_F(SimulatorTest, Run_GeneratesPositivePrices) {
  Config cfg = CreateTestConfig();
  cfg.steps_count = 100;
  cfg.price_variation = 0.5;  // High volatility
  Simulator sim(cfg);

  sim.Run();

  auto ticks = ParseTickLog();
  for (const auto& tick : ticks) {
    EXPECT_GT(tick.price, 0.0)
        << "Price must always be positive (GBM property)";
  }
}

TEST_F(SimulatorTest, Run_PricesVaryOverTime) {
  Config cfg = CreateTestConfig();
  cfg.steps_count = 100;
  cfg.price_variation = 0.2;
  Simulator sim(cfg);

  sim.Run();

  auto ticks = ParseTickLog();
  ASSERT_GT(ticks.size(), 1);

  // Check that not all prices are identical
  bool has_variation = false;
  double first_price = ticks[0].price;
  for (const auto& tick : ticks) {
    if (std::abs(tick.price - first_price) > 0.001) {
      has_variation = true;
      break;
    }
  }
  EXPECT_TRUE(has_variation) << "Prices should vary due to GBM randomness";
}

TEST_F(SimulatorTest, Run_InitialPriceUsed) {
  Config cfg = CreateTestConfig();
  cfg.steps_count = 1;
  cfg.initial_price = 500.0;
  cfg.price_variation = 0.01;  // Low volatility for predictability
  Simulator sim(cfg);

  sim.Run();

  auto ticks = ParseTickLog();
  ASSERT_GE(ticks.size(), 1);
  // First price should be close to initial (with small variation)
  EXPECT_GT(ticks[0].price, 400.0);
  EXPECT_LT(ticks[0].price, 600.0);
}

// ============================================================================
// Run Tests - Volume Generation
// ============================================================================

TEST_F(SimulatorTest, Run_VolumesInRange) {
  Config cfg = CreateTestConfig();
  cfg.steps_count = 100;
  cfg.min_volume = 10.0;
  cfg.max_volume = 50.0;
  Simulator sim(cfg);

  sim.Run();

  auto ticks = ParseTickLog();
  for (const auto& tick : ticks) {
    EXPECT_GE(tick.volume, cfg.min_volume);
    EXPECT_LE(tick.volume, cfg.max_volume);
  }
}

TEST_F(SimulatorTest, Run_VolumeVariation) {
  Config cfg = CreateTestConfig();
  cfg.steps_count = 100;
  cfg.min_volume = 10.0;
  cfg.max_volume = 100.0;
  Simulator sim(cfg);

  sim.Run();

  auto ticks = ParseTickLog();
  ASSERT_GT(ticks.size(), 1);

  // Check that volumes vary
  double min_vol = ticks[0].volume;
  double max_vol = ticks[0].volume;
  for (const auto& tick : ticks) {
    min_vol = std::min(min_vol, tick.volume);
    max_vol = std::max(max_vol, tick.volume);
  }
  EXPECT_GT(max_vol - min_vol, 1.0) << "Volumes should vary randomly";
}

// ============================================================================
// Run Tests - Timestamp
// ============================================================================

TEST_F(SimulatorTest, Run_TimestampsIncreasing) {
  Config cfg = CreateTestConfig();
  cfg.steps_count = 50;
  Simulator sim(cfg);

  sim.Run();

  auto lines = ReadTickLogLines();
  // Simple check: lines should be in order (timestamps are strings)
  // Since timestamps are formatted as HH:MM:SS, string comparison works for
  // ordering
  EXPECT_GT(lines.size(), 2);

  // Parse timestamps to verify ordering
  auto ticks = ParseTickLog();
  for (size_t i = 1; i < ticks.size(); ++i) {
    EXPECT_GE(ticks[i].time, ticks[i - 1].time)
        << "Timestamps should be monotonically increasing";
  }
}

// ============================================================================
// Run Tests - Logger Integration
// ============================================================================

TEST_F(SimulatorTest, Run_WritesTicksToLogger) {
  Config cfg = CreateTestConfig();
  cfg.steps_count = 20;
  Simulator sim(cfg);

  sim.Run();

  // Verify tick log file exists and has content
  EXPECT_TRUE(fs::exists(temp_dir / "ticks.csv"));
  auto lines = ReadTickLogLines();
  EXPECT_EQ(lines.size(), 21);  // Header + 20 ticks
}

TEST_F(SimulatorTest, Run_TickLogHasCorrectHeader) {
  Config cfg = CreateTestConfig();
  cfg.steps_count = 1;
  Simulator sim(cfg);

  sim.Run();

  auto lines = ReadTickLogLines();
  ASSERT_GE(lines.size(), 1);
  EXPECT_THAT(lines[0], HasSubstr("Time"));
  EXPECT_THAT(lines[0], HasSubstr("Price"));
  EXPECT_THAT(lines[0], HasSubstr("Volume"));
}

// ============================================================================
// Run Tests - Bot Integration
// ============================================================================

TEST_F(SimulatorTest, Run_PassesTicksToBot) {
  Config cfg = CreateTestConfig();
  cfg.steps_count = 100;
  cfg.fast_ema = 50ms;
  cfg.slow_ema = 200ms;
  cfg.min_diff_time = 10ms;
  cfg.max_diff_time = 20ms;
  cfg.price_variation = 0.3;  // High volatility for more signals
  Simulator sim(cfg);

  sim.Run();

  // Bot should have received ticks and potentially generated orders
  EXPECT_TRUE(fs::exists(temp_dir / "orders.csv"));
}

// ============================================================================
// GBM Properties Tests
// ============================================================================

TEST_F(SimulatorTest, GBM_ZeroVariation_StablePrices) {
  Config cfg = CreateTestConfig();
  cfg.steps_count = 50;
  cfg.price_variation = 0.0001;   // Near-zero variation
  cfg.average_trend_value = 0.0;  // No trend
  Simulator sim(cfg);

  sim.Run();

  auto ticks = ParseTickLog();
  ASSERT_GT(ticks.size(), 1);

  // With near-zero variation and no trend, prices should be very stable
  double initial = ticks[0].price;
  for (const auto& tick : ticks) {
    EXPECT_NEAR(tick.price, initial, initial * 0.1);
  }
}

// ============================================================================
// Edge Cases Tests
// ============================================================================

TEST_F(SimulatorTest, Run_LargeStepsCount) {
  Config cfg = CreateTestConfig();
  cfg.steps_count = 1000;
  Simulator sim(cfg);

  EXPECT_NO_THROW(sim.Run());

  auto lines = ReadTickLogLines();
  EXPECT_EQ(lines.size(), 1001);  // Header + 1000 ticks
}

TEST_F(SimulatorTest, Run_MinDiffTimeEqualsMaxDiffTime) {
  Config cfg = CreateTestConfig();
  cfg.steps_count = 10;
  cfg.min_diff_time = 100ms;
  cfg.max_diff_time = 100ms;  // Same as min
  Simulator sim(cfg);

  EXPECT_NO_THROW(sim.Run());

  auto lines = ReadTickLogLines();
  EXPECT_EQ(lines.size(), 11);
}

TEST_F(SimulatorTest, Run_VerySmallDiffTime) {
  Config cfg = CreateTestConfig();
  cfg.steps_count = 10;
  cfg.min_diff_time = 1ns;
  cfg.max_diff_time = 10ns;
  Simulator sim(cfg);

  EXPECT_NO_THROW(sim.Run());

  auto lines = ReadTickLogLines();
  EXPECT_EQ(lines.size(), 11);
}

TEST_F(SimulatorTest, Run_HighInitialPrice) {
  Config cfg = CreateTestConfig();
  cfg.steps_count = 10;
  cfg.initial_price = 1e6;
  Simulator sim(cfg);

  sim.Run();

  auto ticks = ParseTickLog();
  for (const auto& tick : ticks) {
    EXPECT_GT(tick.price, 0.0);  // Should still be positive
  }
}

TEST_F(SimulatorTest, Run_LowInitialPrice) {
  Config cfg = CreateTestConfig();
  cfg.steps_count = 10;
  cfg.initial_price = 0.001;
  cfg.price_variation = 0.1;
  Simulator sim(cfg);

  sim.Run();

  auto ticks = ParseTickLog();
  for (const auto& tick : ticks) {
    EXPECT_GT(tick.price, 0.0);
  }
}