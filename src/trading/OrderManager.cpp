#include "OrderManager.h"

OrderManager::OrderManager(const Config& config)
    : exchange_api_(config.rejection_probability),
      logger_(config),
      min_position_(config.min_position),
      max_position_(config.max_position) {}

OrderManager::~OrderManager() = default;

Price OrderManager::getTotalPnL(Price currentMarketPrice) const {
  return pnl_ + currentMarketPrice * current_position_;
}

OrderIdentifier OrderManager::SendOrder(const Order& order) {
  auto order_id = exchange_api_.sendOrder(
      order,
      std::bind(&OrderManager::HandleRequestReply, this, std::placeholders::_1,
                std::placeholders::_2, std::placeholders::_3));
  orders_[order_id] = order;
  exchange_api_.poll();
  return order_id;
}

void OrderManager::onBuySignal(Price price, Volume volume) {
  if (isVolumeEqual(current_position_, max_position_)) {
    return;
  }

  Volume remaining = max_position_ - current_position_;
  Volume volume_to_buy = std::min(volume, remaining);

  if (volume_to_buy <= 0) return;

  SendOrder({OrderSide::Buy, price, volume_to_buy});
}

void OrderManager::onSellSignal(Price price, Volume volume) {
  if (isVolumeEqual(current_position_, min_position_)) {
    return;
  }

  Volume room_to_sell = current_position_ - min_position_;
  Volume volume_to_sell = std::min(volume, room_to_sell);

  if (volume_to_sell <= 0) return;

  SendOrder({OrderSide::Sell, price, volume_to_sell});
}

void OrderManager::fixOrder(OrderSide side, Price price, Volume volume) {
  pnl_ += price * volume * (side == OrderSide::Buy ? -1 : 1);
  current_position_ += volume * (side == OrderSide::Buy ? 1 : -1);
}

void OrderManager::HandleRequestReply(OrderIdentifier id, Status reply_status,
                                      std::string_view reply_error) {
  auto it = orders_.find(id);
  if (it == orders_.end()) {
    return;
  }

  const Order& order = it->second;

  if (reply_status == Status::Executed) {
    fixOrder(order.side, order.price, order.volume);
  }

  logger_.writeOrder(order.side, order.price, order.volume, reply_status,
                     std::string(reply_error), getTotalPnL(order.price));

  orders_.erase(it);
}