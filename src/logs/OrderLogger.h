#ifndef TRADINGSIMULATOR_ORDERLOGGER_H
#define TRADINGSIMULATOR_ORDERLOGGER_H

#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

#include "common/Types.h"
#include "config/Config.h"

namespace fs = std::filesystem;

class OrderLogger {
 public:
  explicit OrderLogger(const Config& config);
  std::optional<std::string> writeOrder(OrderSide order_side, Price price,
                                        Volume volume, Status status,
                                        const std::string& error_text,
                                        Price total_pnl);

 private:
  std::optional<std::string> openFile();

  fs::path file_path_;
  std::ofstream file_;
};

#endif  // TRADINGSIMULATOR_ORDERLOGGER_H
