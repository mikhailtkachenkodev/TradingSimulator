#ifndef TRADINGSIMULATOR_ORDERMANAGER_H
#define TRADINGSIMULATOR_ORDERMANAGER_H
#include <unordered_map>

#include "ExchangeApi.h"
#include "common/Types.h"
#include "logs/OrderLogger.h"

class OrderManager : IHandler {
 public:
  explicit OrderManager(const Config& config);
  ~OrderManager() override;

  OrderIdentifier SendOrder(const Order& order);

  void onBuySignal(Price price, Volume volume);
  void onSellSignal(Price price, Volume volume);

 private:
  void HandleRequestReply(OrderIdentifier id, Status reply_status,
                          std::string_view reply_error) override;
  void fixOrder(OrderSide ordSide, Price price, Volume volume);
  [[nodiscard]] Price getTotalPnL(Price currentMarketPrice) const;

  ExchangeApi exchange_api_;
  std::unordered_map<OrderIdentifier, Order> orders_;
  OrderLogger logger_;
  Price pnl_ = 0;
  Volume current_position_ = 0;

  Volume min_position_;
  Volume max_position_;
};

#endif  // TRADINGSIMULATOR_ORDERMANAGER_H
