#ifndef TRADINGSIMULATOR_TICKLOGGER_H
#define TRADINGSIMULATOR_TICKLOGGER_H

#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

#include "common/Types.h"
#include "config/Config.h"

namespace fs = std::filesystem;

class TickLogger {
 public:
  explicit TickLogger(const Config& config);
  std::optional<std::string> writeTick(const Tick& tick);

 private:
  std::optional<std::string> openFile();

  fs::path file_path_;
  std::ofstream file_;
};

#endif  // TRADINGSIMULATOR_TICKLOGGER_H
