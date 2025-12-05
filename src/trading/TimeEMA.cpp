#include "TimeEMA.h"

using namespace std::chrono_literals;

TimeEMA::TimeEMA(std::chrono::nanoseconds period) {
  const double tau_sec = std::chrono::duration<double>(period).count();
  neg_inv_tau_ = -1.0 / tau_sec;
}

Price TimeEMA::update(const Tick& tick) {
  if (!last_time_update_.has_value()) {
    current_ma_price_ = tick.price;
    last_time_update_ = tick.timestamp;
    return tick.price;
  }

  std::chrono::nanoseconds deltaT = tick.timestamp - *last_time_update_;

  if (deltaT <= 0ns) {
    return current_ma_price_;
  }

  const double dt_sec = std::chrono::duration<double>(deltaT).count();

  // Alpha = 1 - e^(-dt / tau)
  const double alpha = 1.0 - std::exp(dt_sec * neg_inv_tau_);
  current_ma_price_ =
      current_ma_price_ + alpha * (tick.price - current_ma_price_);
  last_time_update_ = tick.timestamp;

  return current_ma_price_;
}

Price TimeEMA::getCurrentPrice() const { return current_ma_price_; }
