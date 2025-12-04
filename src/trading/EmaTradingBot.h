#ifndef TRADINGSIMULATOR_TRADINGBOT_H
#define TRADINGSIMULATOR_TRADINGBOT_H

#include "OrderManager.h"
#include "TimeEMA.h"
#include "common/Types.h"

using namespace std::chrono_literals;

enum class IndicatorHigher { Fast, Slow, None };

class EmaTradingBot {
 public:
  explicit EmaTradingBot(const Config& config);
  void onTick(const Tick& tick);

 private:
  IndicatorHigher higher_ema_ = IndicatorHigher::None;
  TimeEMA fast_ema_;
  TimeEMA slow_ema_;
  OrderManager order_manager_;
};

#endif  // TRADINGSIMULATOR_TRADINGBOT_H
