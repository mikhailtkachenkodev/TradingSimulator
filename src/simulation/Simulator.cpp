#include "Simulator.h"

#include <iostream>
#include <print>

Simulator::Simulator(const Config& config)
    : currentTick_(0ns, config.initial_price, 0),
      logger_(config),
      config_(config),
      tradingBot_(config),
      gen_(std::random_device{}()),
      norm_dist_(0.0, 1.0) {}

void Simulator::Run() {
  for (uint64_t i = 0; i < config_.steps_count; ++i) {
    std::chrono::nanoseconds deltaT = getRandomDeltaT();
    currentTick_.timestamp += deltaT;
    currentTick_.price = calculateGBM(deltaT);
    currentTick_.volume = getRandomVolume();
    auto err = logger_.writeTick(
        {currentTick_.timestamp, currentTick_.price, currentTick_.volume});
    if (err) {
      std::println(stderr, "{}", err.value());
    }
    tradingBot_.onTick(currentTick_);
  }
}

Price Simulator::calculateGBM(std::chrono::nanoseconds deltaT) {
  double t_fraction = static_cast<double>(deltaT.count()) /
                      static_cast<double>(config_.time_horizon.count());

  double Z = norm_dist_(gen_);

  double drift_term = (config_.average_trend_value -
                       0.5 * std::pow(config_.price_variation, 2)) *
                      t_fraction;

  double diffusion_term = config_.price_variation * std::sqrt(t_fraction) * Z;

  return currentTick_.price * std::exp(drift_term + diffusion_term);
}

std::chrono::nanoseconds Simulator::getRandomDeltaT() {
  using RepType = std::chrono::nanoseconds::rep;

  std::uniform_int_distribution<RepType> time_dist(
      config_.min_diff_time.count(), config_.max_diff_time.count());
  RepType random_ticks = time_dist(gen_);

  return std::chrono::nanoseconds(random_ticks);
}

double Simulator::getRandomVolume() {
  std::uniform_real_distribution<double> volume_dist(config_.min_volume,
                                                     config_.max_volume);
  return volume_dist(gen_);
}