#include <gtest/gtest.h>

#include <chrono>

#include "common/Types.h"

using namespace std::chrono_literals;

// ============================================================================
// isVolumeEqual Tests
// ============================================================================

TEST(TypesTest, IsVolumeEqual_SameValues_ReturnsTrue) {
  EXPECT_TRUE(isVolumeEqual(100.0, 100.0));
  EXPECT_TRUE(isVolumeEqual(0.0, 0.0));
  EXPECT_TRUE(isVolumeEqual(-50.0, -50.0));
}

TEST(TypesTest, IsVolumeEqual_VeryCloseValues_ReturnsTrue) {
  // Difference less than 1e-9
  EXPECT_TRUE(isVolumeEqual(100.0, 100.0 + 1e-10));
  EXPECT_TRUE(isVolumeEqual(100.0, 100.0 - 1e-10));
  EXPECT_TRUE(isVolumeEqual(1.0, 1.0 + 5e-10));
}

TEST(TypesTest, IsVolumeEqual_DifferentValues_ReturnsFalse) {
  EXPECT_FALSE(isVolumeEqual(100.0, 101.0));
  EXPECT_FALSE(isVolumeEqual(0.0, 1.0));
  EXPECT_FALSE(isVolumeEqual(-50.0, 50.0));
}

TEST(TypesTest, IsVolumeEqual_ZeroValues_ReturnsTrue) {
  EXPECT_TRUE(isVolumeEqual(0.0, 0.0));
  EXPECT_TRUE(isVolumeEqual(-0.0, 0.0));
  EXPECT_TRUE(isVolumeEqual(0.0, -0.0));
}

TEST(TypesTest, IsVolumeEqual_NearZeroValues_ReturnsTrue) {
  EXPECT_TRUE(isVolumeEqual(1e-10, 0.0));
  EXPECT_TRUE(isVolumeEqual(0.0, 1e-10));
  EXPECT_TRUE(isVolumeEqual(-1e-10, 0.0));
}

TEST(TypesTest, IsVolumeEqual_EdgeCase_ExactlyEpsilonDifference_ReturnsFalse) {
  // Difference exactly at 1e-9 threshold should return false
  EXPECT_FALSE(isVolumeEqual(100.0, 100.0 + 1e-9));
  EXPECT_FALSE(isVolumeEqual(100.0, 100.0 - 1e-9));
}

TEST(TypesTest, IsVolumeEqual_LargeValues_WorksCorrectly) {
  EXPECT_TRUE(isVolumeEqual(1e15, 1e15));
  EXPECT_FALSE(isVolumeEqual(1e15, 1e15 + 1.0));
}

// ============================================================================
// Order Struct Tests
// ============================================================================

TEST(TypesTest, Order_ConstructionAndFields_Buy) {
  Order order{OrderSide::Buy, 100.5, 50.0};

  EXPECT_EQ(order.side, OrderSide::Buy);
  EXPECT_DOUBLE_EQ(order.price, 100.5);
  EXPECT_DOUBLE_EQ(order.volume, 50.0);
}

TEST(TypesTest, Order_ConstructionAndFields_Sell) {
  Order order{OrderSide::Sell, 99.25, 25.5};

  EXPECT_EQ(order.side, OrderSide::Sell);
  EXPECT_DOUBLE_EQ(order.price, 99.25);
  EXPECT_DOUBLE_EQ(order.volume, 25.5);
}

// ============================================================================
// Tick Struct Tests
// ============================================================================

TEST(TypesTest, Tick_ConstructionAndFields) {
  Tick tick{1000ns, 150.75, 100.0};

  EXPECT_EQ(tick.timestamp, 1000ns);
  EXPECT_DOUBLE_EQ(tick.price, 150.75);
  EXPECT_DOUBLE_EQ(tick.volume, 100.0);
}

TEST(TypesTest, Tick_LargeTimestamp) {
  auto timestamp = std::chrono::hours(24);
  Tick tick{timestamp, 100.0, 50.0};

  EXPECT_EQ(tick.timestamp, std::chrono::duration_cast<std::chrono::nanoseconds>(
                                std::chrono::hours(24)));
}

TEST(TypesTest, Tick_ZeroTimestamp) {
  Tick tick{0ns, 100.0, 50.0};

  EXPECT_EQ(tick.timestamp, 0ns);
}

// ============================================================================
// Enum Tests
// ============================================================================

TEST(TypesTest, OrderSide_EnumValues) {
  EXPECT_NE(OrderSide::Buy, OrderSide::Sell);
}

TEST(TypesTest, Status_EnumValues) {
  EXPECT_NE(Status::Pending, Status::Executed);
  EXPECT_NE(Status::Pending, Status::Rejected);
  EXPECT_NE(Status::Executed, Status::Rejected);
}
