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

  void onBuySignal(Price price, Volume volume);
  void onSellSignal(Price price, Volume volume);

 private:
  void HandleRequestReply(OrderIdentifier id, ReplyStatus reply_status,
                          std::string_view reply_error) override;
  void fixOrder(OrderSide ordSide, Price price, Volume volume);
  [[nodiscard]] Price getTotalPnL(Price currentMarketPrice) const;

  ExchangeApi exchange_api_;
  std::unordered_map<OrderIdentifier, Order> orders_;
  OrderLogger logger_;
  Price pnl = 0;
  Volume current_volume_ = 0;

  Volume min_position_;
  Volume max_position_;
};

#endif  // TRADINGSIMULATOR_ORDERMANAGER_H
