#ifndef TRADINGSIMULATOR_SIMULATOR_H
#define TRADINGSIMULATOR_SIMULATOR_H

#include <chrono>
#include <random>

#include "common/Types.h"
#include "config/Config.h"
#include "logs/TickLogger.h"
#include "trading/EmaTradingBot.h"

using namespace std::chrono_literals;

class Simulator {
 public:
  explicit Simulator(const Config& config);
  void Run();

 private:
  Price calculateGBM(std::chrono::nanoseconds deltaT);
  std::chrono::nanoseconds getRandomDeltaT();
  double getRandomVolume();

  Tick currentTick_;
  TickLogger logger_;
  Config config_;
  EmaTradingBot tradingBot_;

  std::mt19937 gen_;
  std::normal_distribution<double> norm_dist_;
};

#endif  // TRADINGSIMULATOR_SIMULATOR_H
