#include "OrderManager.h"

OrderManager::OrderManager(const Config& config)
    : exchange_api_(config.rejection_probability),
      logger_(config),
      min_position_(config.min_position),
      max_position_(config.max_position) {}

OrderManager::~OrderManager() = default;

Price OrderManager::getTotalPnL(Price currentMarketPrice) const {
  return pnl + currentMarketPrice * current_volume_;
}

void OrderManager::onBuySignal(Price price, Volume volume) {
  if (isVolumeEqual(current_volume_, max_position_)) {
    return;
  }

  Volume remaining_volume = max_position_ - current_volume_;
  Volume volume_to_buy = std::min(volume, remaining_volume);

  if (volume_to_buy <= 0) return;

  auto ord_id = exchange_api_.sendOrder(
      {OrderSide::Buy, price, volume_to_buy},
      std::bind(&OrderManager::HandleRequestReply, this, std::placeholders::_1,
                std::placeholders::_2, std::placeholders::_3));
  orders_[ord_id] = {OrderSide::Buy, price, volume_to_buy};
  exchange_api_.poll();
}

void OrderManager::onSellSignal(Price price, Volume volume) {
  if (isVolumeEqual(current_volume_, min_position_)) {
    return;
  }

  Volume room_to_sell = current_volume_ - min_position_;
  Volume volume_to_sell = std::min(volume, room_to_sell);

  if (volume_to_sell <= 0) return;

  auto ord_id = exchange_api_.sendOrder(
      {OrderSide::Sell, price, volume_to_sell},
      std::bind(&OrderManager::HandleRequestReply, this, std::placeholders::_1,
                std::placeholders::_2, std::placeholders::_3));
  orders_[ord_id] = {OrderSide::Sell, price, volume_to_sell};
  exchange_api_.poll();
}

void OrderManager::fixOrder(OrderSide ordSide, Price price, Volume volume) {
  pnl += price * volume * (ordSide == OrderSide::Buy ? -1 : 1);
  current_volume_ += volume * (ordSide == OrderSide::Buy ? 1 : -1);
}

void OrderManager::HandleRequestReply(OrderIdentifier id,
                                      ReplyStatus reply_status,
                                      std::string_view reply_error) {
  Order order = orders_[id];

  if (reply_status == ReplyStatus::Executed) {
    fixOrder(order.side, order.price, order.volume);
  }

  logger_.writeOrder(order.side, order.price, order.volume, reply_status,
                     std::string(reply_error), getTotalPnL(order.price));

  orders_.erase(id);
}