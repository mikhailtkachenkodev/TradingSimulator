#include "TickLogger.h"

#include <fstream>
#include <iostream>

TickLogger::TickLogger(const Config& config)
    : file_path_(config.price_evolution_path) {
  auto error = openFile();
  if (error) {
    throw std::runtime_error(error.value());
  }
}

std::optional<std::string> TickLogger::writeTick(const Tick& tick) {
  auto timestamp_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(tick.timestamp);

  file_ << std::format("{:%T},{:.3f},{:.3f}\n", timestamp_ms, tick.price,
                       tick.volume);

  if (file_.bad()) {
    return std::format("TickLogger: critical file write error");
  }
  return std::nullopt;
}

std::optional<std::string> TickLogger::openFile() {
  std::error_code ec;
  fs::create_directories(file_path_.parent_path(), ec);

  if (ec) {
    return std::format("TickLogger: error on folder creation for path: {}",
                       file_path_.string());  // Выход с кодом ошибки
  }

  file_.open(file_path_);

  if (!file_) {
    return std::format("TickLogger: error on file open for path: {}",
                       file_path_.string());
  }

  file_ << std::format("{},{},{}\n", "Time", "Price", "Volume");

  if (file_.bad()) {
    return std::format("TickLogger: critical file write error");
  }

  return std::nullopt;
}