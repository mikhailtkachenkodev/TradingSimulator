#include <gtest/gtest.h>

#include <chrono>
#include <cmath>

#include "trading/TimeEMA.h"

using namespace std::chrono_literals;

// ============================================================================
// Constructor Tests
// ============================================================================

TEST(TimeEMATest, Constructor_ValidPeriod_CreatesEMA) {
  EXPECT_NO_THROW(TimeEMA ema(1s));
  EXPECT_NO_THROW(TimeEMA ema(100ms));
  EXPECT_NO_THROW(TimeEMA ema(24h));
}

TEST(TimeEMATest, Constructor_SmallPeriod_CreatesEMA) {
  EXPECT_NO_THROW(TimeEMA ema(1ns));
  EXPECT_NO_THROW(TimeEMA ema(1us));
}

// ============================================================================
// GetCurrentPrice Tests
// ============================================================================

TEST(TimeEMATest, GetCurrentPrice_BeforeUpdate_ReturnsZero) {
  TimeEMA ema(1s);

  EXPECT_DOUBLE_EQ(ema.getCurrentPrice(), 0.0);
}

// ============================================================================
// Update - First Tick Tests
// ============================================================================

TEST(TimeEMATest, Update_FirstTick_ReturnsTickPrice) {
  TimeEMA ema(1s);
  Tick tick{100ms, 150.0, 100.0};

  Price result = ema.update(tick);

  EXPECT_DOUBLE_EQ(result, 150.0);
}

TEST(TimeEMATest, Update_FirstTick_SetsCurrentPrice) {
  TimeEMA ema(1s);
  Tick tick{100ms, 150.0, 100.0};

  ema.update(tick);

  EXPECT_DOUBLE_EQ(ema.getCurrentPrice(), 150.0);
}

TEST(TimeEMATest, Update_FirstTick_ZeroTimestamp_Works) {
  TimeEMA ema(1s);
  Tick tick{0ns, 100.0, 50.0};

  Price result = ema.update(tick);

  EXPECT_DOUBLE_EQ(result, 100.0);
  EXPECT_DOUBLE_EQ(ema.getCurrentPrice(), 100.0);
}

// ============================================================================
// Update - Subsequent Ticks Tests
// ============================================================================

TEST(TimeEMATest, Update_SecondTick_CalculatesEMA) {
  TimeEMA ema(1s);

  ema.update({0ns, 100.0, 50.0});
  Price result = ema.update({500ms, 200.0, 50.0});

  // alpha = 1 - exp(-0.5/1.0) = 1 - exp(-0.5) ≈ 0.3935
  // EMA = 100 + 0.3935 * (200 - 100) ≈ 139.35
  EXPECT_GT(result, 100.0);
  EXPECT_LT(result, 200.0);
}

TEST(TimeEMATest, Update_ZeroDeltaTime_ReturnsPreviousPrice) {
  TimeEMA ema(1s);

  ema.update({100ms, 100.0, 50.0});
  Price result = ema.update({100ms, 200.0, 50.0});  // Same timestamp

  EXPECT_DOUBLE_EQ(result, 100.0);
  EXPECT_DOUBLE_EQ(ema.getCurrentPrice(), 100.0);
}

TEST(TimeEMATest, Update_NegativeDeltaTime_ReturnsPreviousPrice) {
  TimeEMA ema(1s);

  ema.update({200ms, 100.0, 50.0});
  Price result = ema.update({100ms, 200.0, 50.0});  // Earlier timestamp

  EXPECT_DOUBLE_EQ(result, 100.0);
  EXPECT_DOUBLE_EQ(ema.getCurrentPrice(), 100.0);
}

TEST(TimeEMATest, Update_SmallDeltaTime_SmallAlpha) {
  TimeEMA ema(1s);

  ema.update({0ns, 100.0, 50.0});
  Price result = ema.update({10ms, 200.0, 50.0});  // Very small delta

  // alpha = 1 - exp(-0.01/1.0) ≈ 0.00995
  // EMA should be very close to original price
  EXPECT_GT(result, 100.0);
  EXPECT_LT(result, 102.0);  // Small change due to small alpha
}

TEST(TimeEMATest, Update_LargeDeltaTime_ConvergesToNewPrice) {
  TimeEMA ema(1s);

  ema.update({0ns, 100.0, 50.0});
  Price result = ema.update({10s, 200.0, 50.0});  // Large delta (10 * tau)

  // alpha = 1 - exp(-10) ≈ 0.99995
  // EMA should be very close to new price
  EXPECT_GT(result, 199.0);
  EXPECT_LE(result, 200.0);
}

TEST(TimeEMATest, Update_MultipleUpdates_Convergence) {
  TimeEMA ema(100ms);

  ema.update({0ms, 100.0, 50.0});

  // Multiple updates towards 200.0
  for (int i = 1; i <= 10; ++i) {
    ema.update({std::chrono::milliseconds(i * 100), 200.0, 50.0});
  }

  // After many tau periods, should be very close to 200
  EXPECT_GT(ema.getCurrentPrice(), 199.0);
}

TEST(TimeEMATest, Update_SamePrice_NoChange) {
  TimeEMA ema(1s);

  ema.update({0ns, 100.0, 50.0});
  Price result = ema.update({500ms, 100.0, 50.0});

  EXPECT_DOUBLE_EQ(result, 100.0);
}

TEST(TimeEMATest, Update_PriceIncrease_MAIncreases) {
  TimeEMA ema(1s);

  ema.update({0ns, 100.0, 50.0});
  Price result = ema.update({500ms, 150.0, 50.0});

  EXPECT_GT(result, 100.0);
  EXPECT_LT(result, 150.0);
}

TEST(TimeEMATest, Update_PriceDecrease_MADecreases) {
  TimeEMA ema(1s);

  ema.update({0ns, 100.0, 50.0});
  Price result = ema.update({500ms, 50.0, 50.0});

  EXPECT_LT(result, 100.0);
  EXPECT_GT(result, 50.0);
}

// ============================================================================
// Edge Cases Tests
// ============================================================================

TEST(TimeEMATest, Update_ExtremePrices_HandlesCorrectly) {
  TimeEMA ema(1s);

  ema.update({0ns, 1e10, 50.0});
  Price result = ema.update({500ms, 1e10 * 2, 50.0});

  EXPECT_GT(result, 1e10);
  EXPECT_LT(result, 2e10);
}

TEST(TimeEMATest, Update_VerySmallPeriod_FastConvergence) {
  TimeEMA ema(1ms);  // Very small period

  ema.update({0ns, 100.0, 50.0});
  Price result = ema.update({10ms, 200.0, 50.0});  // 10 * tau

  // Should converge very quickly
  EXPECT_GT(result, 199.0);
}

TEST(TimeEMATest, Update_VeryLargePeriod_SlowConvergence) {
  TimeEMA ema(1h);  // Very large period

  ema.update({0ns, 100.0, 50.0});
  Price result = ema.update({1s, 200.0, 50.0});  // Small fraction of tau

  // Should move very slowly towards new price
  EXPECT_GT(result, 100.0);
  EXPECT_LT(result, 101.0);  // Very small change
}

TEST(TimeEMATest, Update_ZeroPrice_Works) {
  TimeEMA ema(1s);

  ema.update({0ns, 0.0, 50.0});
  Price result = ema.update({500ms, 100.0, 50.0});

  EXPECT_GT(result, 0.0);
  EXPECT_LT(result, 100.0);
}

TEST(TimeEMATest, Update_AlternatingPrices_Smoothing) {
  TimeEMA ema(100ms);

  ema.update({0ms, 100.0, 50.0});
  ema.update({50ms, 200.0, 50.0});
  ema.update({100ms, 100.0, 50.0});
  Price result = ema.update({150ms, 200.0, 50.0});

  // EMA should smooth out the oscillations
  EXPECT_GT(result, 100.0);
  EXPECT_LT(result, 200.0);
}

// ============================================================================
// EMA Formula Verification
// ============================================================================

TEST(TimeEMATest, Update_FormulaVerification) {
  TimeEMA ema(1s);
  const Price initial_price = 100.0;
  const Price new_price = 200.0;
  const auto delta_time = 500ms;

  ema.update({0ns, initial_price, 50.0});
  Price result = ema.update({delta_time, new_price, 50.0});

  // Manual calculation
  double dt_sec = 0.5;  // 500ms = 0.5s
  double tau_sec = 1.0;  // 1s
  double alpha = 1.0 - std::exp(-dt_sec / tau_sec);
  double expected = initial_price + alpha * (new_price - initial_price);

  EXPECT_NEAR(result, expected, 1e-9);
}
