#include "EmaTradingBot.h"

EmaTradingBot::EmaTradingBot(const Config& config)
    : fast_ema_(config.fast_ema),
      slow_ema_(config.slow_ema),
      order_manager_(config) {}

void EmaTradingBot::onTick(const Tick& tick) {
  slow_ema_.update(tick);
  fast_ema_.update(tick);

  if (fast_ema_.getCurrentPrice() > slow_ema_.getCurrentPrice()) {
    if (higher_ema_ == IndicatorHigher::Slow) {
      order_manager_.onBuySignal(tick.price, tick.volume);
    }
    higher_ema_ = IndicatorHigher::Fast;
    return;
  }

  if (higher_ema_ == IndicatorHigher::Fast) {
    order_manager_.onSellSignal(tick.price, tick.volume);
  }
  higher_ema_ = IndicatorHigher::Slow;
}