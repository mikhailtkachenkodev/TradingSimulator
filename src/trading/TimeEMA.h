#ifndef TRADINGSIMULATOR_TIMEEMA_H
#define TRADINGSIMULATOR_TIMEEMA_H

#include <chrono>

#include "common/Types.h"

using namespace std::chrono_literals;

class TimeEMA {
 public:
  explicit TimeEMA(std::chrono::nanoseconds period);
  Price update(const Tick& tick);

  [[nodiscard]] Price getCurrentPrice() const;

 private:
  Price current_ma_price_ = 0;
  std::chrono::nanoseconds last_time_update_ = -1ns;
  double neg_inv_tau_;
};

#endif  // TRADINGSIMULATOR_TIMEEMA_H
