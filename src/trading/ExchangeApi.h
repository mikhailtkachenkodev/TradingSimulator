#ifndef TRADINGSIMULATOR_EXCHANGEAPI_H
#define TRADINGSIMULATOR_EXCHANGEAPI_H

#include <functional>
#include <random>
#include <string_view>

#include "common/Types.h"

using ExchangeCallback =
    std::function<void(OrderIdentifier, ReplyStatus, std::string_view)>;

class ExchangeApi {
 public:
  explicit ExchangeApi(double rejection_percent);
  OrderIdentifier sendOrder(const Order& order, ExchangeCallback cb);

  void poll();

 private:
  struct PendingEvent {
    OrderIdentifier id;
    ReplyStatus reply_status;
    ExchangeCallback cb;
  };

  std::vector<PendingEvent> pending_events_;
  double rejection_percent_;
  std::mt19937 rng_;
  OrderIdentifier nextId_ = 1;
};

#endif  // TRADINGSIMULATOR_EXCHANGEAPI_H
