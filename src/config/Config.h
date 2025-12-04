#ifndef TRADINGSIMULATOR_CONFIG_H
#define TRADINGSIMULATOR_CONFIG_H

#include <chrono>
#include <cstdint>
#include <filesystem>

using namespace std::chrono_literals;

#include "common/Types.h"

struct Config {
  // Price
  Price initial_price = 100;
  double average_trend_value = 0.05;
  double price_variation = 0.10;
  std::chrono::nanoseconds time_horizon = 24h;
  std::chrono::nanoseconds min_diff_time = 100ms;
  std::chrono::nanoseconds max_diff_time = 200ms;

  // Trade
  std::chrono::nanoseconds fast_ema = 1s;
  std::chrono::nanoseconds slow_ema = 5s;
  Volume min_volume = 1;
  Volume max_volume = 1000;
  Volume min_position = -1000;
  Volume max_position = 1000;

  // Exchange
  double rejection_probability = 1.0;

  // Simulation
  uint64_t steps_count = 100000;
  std::filesystem::path price_evolution_path = "output/price_evolution.csv";
  std::filesystem::path orders_log_path = "output/orders.csv";
};

#endif  // TRADINGSIMULATOR_CONFIG_H
