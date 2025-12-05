#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <set>
#include <vector>

#include "trading/ExchangeApi.h"

using ::testing::_;

// ============================================================================
// Constructor Tests
// ============================================================================

TEST(ExchangeApiTest, Constructor_ValidRejectionPercent_Creates) {
  EXPECT_NO_THROW(ExchangeApi api(0.0));
  EXPECT_NO_THROW(ExchangeApi api(50.0));
  EXPECT_NO_THROW(ExchangeApi api(100.0));
}

// ============================================================================
// SendOrder - ID Generation Tests
// ============================================================================

TEST(ExchangeApiTest, SendOrder_FirstOrder_ReturnsId1) {
  ExchangeApi api(0.0);
  Order order{OrderSide::Buy, 100.0, 50.0};

  OrderIdentifier id = api.sendOrder(order, [](auto, auto, auto) {});

  EXPECT_EQ(id, 1);
}

TEST(ExchangeApiTest, SendOrder_ReturnsIncrementingIds) {
  ExchangeApi api(0.0);
  Order order{OrderSide::Buy, 100.0, 50.0};

  OrderIdentifier id1 = api.sendOrder(order, [](auto, auto, auto) {});
  OrderIdentifier id2 = api.sendOrder(order, [](auto, auto, auto) {});
  OrderIdentifier id3 = api.sendOrder(order, [](auto, auto, auto) {});

  EXPECT_EQ(id1, 1);
  EXPECT_EQ(id2, 2);
  EXPECT_EQ(id3, 3);
}

TEST(ExchangeApiTest, SendOrder_MultipleOrders_UniqueIds) {
  ExchangeApi api(0.0);
  Order order{OrderSide::Buy, 100.0, 50.0};
  std::set<OrderIdentifier> ids;

  for (int i = 0; i < 100; ++i) {
    ids.insert(api.sendOrder(order, [](auto, auto, auto) {}));
  }

  EXPECT_EQ(ids.size(), 100);
}

// ============================================================================
// SendOrder - Callback Storage Tests
// ============================================================================

TEST(ExchangeApiTest, SendOrder_StoresCallbackForPoll) {
  ExchangeApi api(0.0);
  Order order{OrderSide::Buy, 100.0, 50.0};
  bool callback_called = false;

  api.sendOrder(order, [&callback_called](auto, auto, auto) {
    callback_called = true;
  });

  EXPECT_FALSE(callback_called);  // Not called yet

  api.poll();

  EXPECT_TRUE(callback_called);  // Called after poll
}

TEST(ExchangeApiTest, SendOrder_WithNullCallback_DoesNotCrash) {
  ExchangeApi api(0.0);
  Order order{OrderSide::Buy, 100.0, 50.0};

  EXPECT_NO_THROW(api.sendOrder(order, nullptr));
  EXPECT_NO_THROW(api.poll());
}

// ============================================================================
// Poll Tests
// ============================================================================

TEST(ExchangeApiTest, Poll_WithNoPendingEvents_DoesNothing) {
  ExchangeApi api(0.0);

  EXPECT_NO_THROW(api.poll());
}

TEST(ExchangeApiTest, Poll_WithPendingEvent_InvokesCallback) {
  ExchangeApi api(0.0);
  Order order{OrderSide::Buy, 100.0, 50.0};
  int callback_count = 0;

  api.sendOrder(order, [&callback_count](auto, auto, auto) { ++callback_count; });
  api.poll();

  EXPECT_EQ(callback_count, 1);
}

TEST(ExchangeApiTest, Poll_ClearsPendingEventsAfterExecution) {
  ExchangeApi api(0.0);
  Order order{OrderSide::Buy, 100.0, 50.0};
  int callback_count = 0;

  api.sendOrder(order, [&callback_count](auto, auto, auto) { ++callback_count; });
  api.poll();
  api.poll();  // Second poll should not call callback again

  EXPECT_EQ(callback_count, 1);
}

TEST(ExchangeApiTest, Poll_CallbackReceivesCorrectOrderId) {
  ExchangeApi api(0.0);
  Order order{OrderSide::Buy, 100.0, 50.0};
  OrderIdentifier received_id = 0;

  OrderIdentifier sent_id = api.sendOrder(
      order, [&received_id](OrderIdentifier id, auto, auto) { received_id = id; });
  api.poll();

  EXPECT_EQ(received_id, sent_id);
}

TEST(ExchangeApiTest, Poll_MultipleOrders_AllCallbacksInvoked) {
  ExchangeApi api(0.0);
  Order order{OrderSide::Buy, 100.0, 50.0};
  std::vector<OrderIdentifier> received_ids;

  api.sendOrder(order, [&received_ids](OrderIdentifier id, auto, auto) {
    received_ids.push_back(id);
  });
  api.sendOrder(order, [&received_ids](OrderIdentifier id, auto, auto) {
    received_ids.push_back(id);
  });
  api.sendOrder(order, [&received_ids](OrderIdentifier id, auto, auto) {
    received_ids.push_back(id);
  });

  api.poll();

  EXPECT_EQ(received_ids.size(), 3);
  EXPECT_EQ(received_ids[0], 1);
  EXPECT_EQ(received_ids[1], 2);
  EXPECT_EQ(received_ids[2], 3);
}

// ============================================================================
// Rejection Rate Tests - 0%
// ============================================================================

TEST(ExchangeApiTest, RejectionRate_0Percent_AllExecuted) {
  ExchangeApi api(0.0);  // 0% rejection
  Order order{OrderSide::Buy, 100.0, 50.0};
  int executed_count = 0;

  for (int i = 0; i < 100; ++i) {
    api.sendOrder(order, [&executed_count](auto, Status status, auto) {
      if (status == Status::Executed) ++executed_count;
    });
  }
  api.poll();

  EXPECT_EQ(executed_count, 100);
}

// ============================================================================
// Rejection Rate Tests - 100%
// ============================================================================

TEST(ExchangeApiTest, RejectionRate_100Percent_AllRejected) {
  ExchangeApi api(100.0);  // 100% rejection
  Order order{OrderSide::Buy, 100.0, 50.0};
  int rejected_count = 0;

  for (int i = 0; i < 100; ++i) {
    api.sendOrder(order, [&rejected_count](auto, Status status, auto) {
      if (status == Status::Rejected) ++rejected_count;
    });
  }
  api.poll();

  EXPECT_EQ(rejected_count, 100);
}

// ============================================================================
// Status and Error Message Tests
// ============================================================================

TEST(ExchangeApiTest, Poll_ExecutedOrder_StatusIsExecuted) {
  ExchangeApi api(0.0);
  Order order{OrderSide::Buy, 100.0, 50.0};
  Status received_status = Status::Pending;

  api.sendOrder(order, [&received_status](auto, Status status, auto) {
    received_status = status;
  });
  api.poll();

  EXPECT_EQ(received_status, Status::Executed);
}

TEST(ExchangeApiTest, Poll_RejectedOrder_StatusIsRejected) {
  ExchangeApi api(100.0);  // 100% rejection
  Order order{OrderSide::Buy, 100.0, 50.0};
  Status received_status = Status::Pending;

  api.sendOrder(order, [&received_status](auto, Status status, auto) {
    received_status = status;
  });
  api.poll();

  EXPECT_EQ(received_status, Status::Rejected);
}

TEST(ExchangeApiTest, Poll_ExecutedOrder_EmptyErrorMessage) {
  ExchangeApi api(0.0);
  Order order{OrderSide::Buy, 100.0, 50.0};
  std::string received_error;

  api.sendOrder(order, [&received_error](auto, auto, std::string_view error) {
    received_error = std::string(error);
  });
  api.poll();

  EXPECT_TRUE(received_error.empty());
}

TEST(ExchangeApiTest, Poll_RejectedOrder_HasErrorMessage) {
  ExchangeApi api(100.0);  // 100% rejection
  Order order{OrderSide::Buy, 100.0, 50.0};
  std::string received_error;

  api.sendOrder(order, [&received_error](auto, auto, std::string_view error) {
    received_error = std::string(error);
  });
  api.poll();

  EXPECT_EQ(received_error, "Random rejection");
}

// ============================================================================
// Statistical Tests - 50% Rejection Rate
// ============================================================================

TEST(ExchangeApiTest, RejectionRate_50Percent_Statistical) {
  ExchangeApi api(50.0);  // 50% rejection
  Order order{OrderSide::Buy, 100.0, 50.0};
  int executed_count = 0;
  int rejected_count = 0;
  const int total_orders = 1000;

  for (int i = 0; i < total_orders; ++i) {
    api.sendOrder(
        order, [&executed_count, &rejected_count](auto, Status status, auto) {
          if (status == Status::Executed)
            ++executed_count;
          else if (status == Status::Rejected)
            ++rejected_count;
        });
  }
  api.poll();

  // With 50% rejection, expect roughly half executed, half rejected
  // Using a generous tolerance for randomness (30-70% range)
  double executed_ratio =
      static_cast<double>(executed_count) / static_cast<double>(total_orders);
  EXPECT_GT(executed_ratio, 0.30);
  EXPECT_LT(executed_ratio, 0.70);
  EXPECT_EQ(executed_count + rejected_count, total_orders);
}

// ============================================================================
// Order Preservation Tests
// ============================================================================

TEST(ExchangeApiTest, Poll_OrderPreservesCallbackParams) {
  ExchangeApi api(0.0);
  Order buy_order{OrderSide::Buy, 100.0, 50.0};
  Order sell_order{OrderSide::Sell, 150.0, 25.0};

  std::vector<std::pair<OrderIdentifier, Status>> results;

  api.sendOrder(buy_order, [&results](OrderIdentifier id, Status status, auto) {
    results.emplace_back(id, status);
  });
  api.sendOrder(sell_order,
                [&results](OrderIdentifier id, Status status, auto) {
                  results.emplace_back(id, status);
                });

  api.poll();

  ASSERT_EQ(results.size(), 2);
  EXPECT_EQ(results[0].first, 1);
  EXPECT_EQ(results[1].first, 2);
}

// ============================================================================
// Multiple Poll Cycles Tests
// ============================================================================

TEST(ExchangeApiTest, MultiplePollCycles_IndependentBatches) {
  ExchangeApi api(0.0);
  Order order{OrderSide::Buy, 100.0, 50.0};
  std::vector<OrderIdentifier> batch1_ids;
  std::vector<OrderIdentifier> batch2_ids;

  // First batch
  api.sendOrder(order, [&batch1_ids](OrderIdentifier id, auto, auto) {
    batch1_ids.push_back(id);
  });
  api.poll();

  // Second batch
  api.sendOrder(order, [&batch2_ids](OrderIdentifier id, auto, auto) {
    batch2_ids.push_back(id);
  });
  api.poll();

  ASSERT_EQ(batch1_ids.size(), 1);
  ASSERT_EQ(batch2_ids.size(), 1);
  EXPECT_EQ(batch1_ids[0], 1);
  EXPECT_EQ(batch2_ids[0], 2);
}
