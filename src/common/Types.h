#ifndef TRADINGSIMULATOR_TYPES_H
#define TRADINGSIMULATOR_TYPES_H

#include <chrono>
#include <cstdint>
#include <string_view>

using Price = double;
using Volume = double;
using OrderIdentifier = uint64_t;

enum class OrderSide { Buy, Sell };

enum class ReplyStatus { Pending, Executed, Rejected };

inline bool isVolumeEqual(const Volume a, const Volume b) {
  return std::abs(a - b) < 1e-9;
}

struct Order {
  OrderSide side;
  Price price;
  Volume volume;
  std::chrono::nanoseconds timestamp;
};

struct Tick {
  std::chrono::nanoseconds timestamp;
  Price price;
  Volume volume;
};

struct IHandler {
  virtual ~IHandler() = default;
  virtual void HandleRequestReply(OrderIdentifier id, ReplyStatus reply_status,
                                  std::string_view reply_error) = 0;
};
#endif  // TRADINGSIMULATOR_TYPES_H
