#include "ExchangeApi.h"
ExchangeApi::ExchangeApi(double rejection_percent)
    : rejection_percent_(rejection_percent), rng_(std::random_device{}()) {}

OrderIdentifier ExchangeApi::sendOrder(const Order& order,
                                       ExchangeCallback cb) {
  const OrderIdentifier current_id = nextId_++;

  std::uniform_real_distribution<double> dist(0.0, 100.0);
  const ReplyStatus rp_status = dist(rng_) < rejection_percent_
                                    ? ReplyStatus::Rejected
                                    : ReplyStatus::Executed;

  pending_events_.push_back(
      {.id = current_id, .reply_status = rp_status, .cb = std::move(cb)});

  return current_id;
}

void ExchangeApi::poll() {
  for (const auto& [id, reply_status, cb] : pending_events_) {
    if (cb) {
      cb(id, reply_status,
         reply_status == ReplyStatus::Rejected ? "Random rejection" : "");
    }
  }

  pending_events_.clear();
}