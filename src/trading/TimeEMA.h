#ifndef TRADINGSIMULATOR_TIMEEMA_H
#define TRADINGSIMULATOR_TIMEEMA_H

#include <chrono>
#include <optional>

#include "common/Types.h"

class TimeEMA {
 public:
  explicit TimeEMA(std::chrono::nanoseconds period);
  Price update(const Tick& tick);

  [[nodiscard]] Price getCurrentPrice() const;

 private:
  Price current_ma_price_ = 0;
  std::optional<std::chrono::nanoseconds> last_time_update_;
  double neg_inv_tau_;
};

#endif  // TRADINGSIMULATOR_TIMEEMA_H
