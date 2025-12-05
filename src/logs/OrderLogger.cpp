#include "OrderLogger.h"

OrderLogger::OrderLogger(const Config& config)
    : file_path_(config.orders_log_path) {
  auto error = openFile();
  if (error) {
    throw std::runtime_error(error.value());
  }
}

std::optional<std::string> OrderLogger::writeOrder(
    OrderSide order_side, Price price, Volume volume, Status status,
    const std::string& error_text, Price total_pnl) {
  auto order_side_string = order_side == OrderSide::Buy ? "Buy" : "Sell";
  std::string status_string;
  switch (status) {
    case Status::Executed:
      status_string = "Executed";
      break;
    case Status::Rejected:
      status_string = "Rejected";
      break;
    case Status::Pending:
      status_string = "Pending";
      break;
  }
  file_ << std::format("{},{:.3f},{:.3f},{},{},{:.3f}", order_side_string,
                       price, volume, status_string, error_text, total_pnl)
        << std::endl;

  if (file_.fail()) {
    return std::format("OrderLogger: file write error");
  }

  return std::nullopt;
}

std::optional<std::string> OrderLogger::openFile() {
  std::error_code ec;
  fs::create_directories(file_path_.parent_path(), ec);

  if (ec) {
    return std::format("OrderLogger: error on folder creation for path: {}",
                       file_path_.string());
  }

  file_.open(file_path_);

  if (!file_) {
    return std::format("OrderLogger: error on file open for path: {}",
                       file_path_.string());
  }

  file_ << std::format("{},{},{},{},{},{}\n", "Side", "Price", "Volume",
                       "ReplyStatus", "ErrorText", "PnL");

  if (file_.fail()) {
    return std::format("OrderLogger: file write error");
  }

  return std::nullopt;
}