#include "ConfigManager.h"

#include <charconv>
#include <format>
#include <regex>

#include "ini.h"

namespace {

std::expected<std::chrono::nanoseconds, std::string> ParseDuration(
    std::string_view input) {
  if (input.empty()) {
    return std::unexpected("Empty duration string");
  }

  std::string s(input);
  // Remove whitespace
  std::erase_if(s, ::isspace);

  std::regex re(R"(^(\d+)(y|m|d|h|min|s|ms|us|ns)$)");
  std::smatch match;

  if (!std::regex_match(s, match, re)) {
    return std::unexpected(std::format("Invalid duration format: {}", input));
  }

  long long value = 0;
  const char* first = &*match[1].first;
  const char* last = first + match[1].length();
  auto result = std::from_chars(first, last, value);
  if (result.ec != std::errc()) {
    return std::unexpected(
        std::format("Invalid number in duration: {}", input));
  }

  std::string suffix = match[2].str();

  using namespace std::chrono;

  if (suffix == "ns") return nanoseconds(value);
  if (suffix == "us") return duration_cast<nanoseconds>(microseconds(value));
  if (suffix == "ms") return duration_cast<nanoseconds>(milliseconds(value));
  if (suffix == "s") return duration_cast<nanoseconds>(seconds(value));
  if (suffix == "min") return duration_cast<nanoseconds>(minutes(value));
  if (suffix == "h") return duration_cast<nanoseconds>(hours(value));
  if (suffix == "d") return duration_cast<nanoseconds>(days(value));
  if (suffix == "m") return duration_cast<nanoseconds>(months(value));
  if (suffix == "y") return duration_cast<nanoseconds>(years(value));

  return std::unexpected(std::format("Unknown time suffix: {}", suffix));
}

std::string DurationToString(std::chrono::nanoseconds ns) {
  using namespace std::chrono;

  // Try to find the largest unit that divides evenly or is appropriate
  if (ns.count() == 0) return "0ns";

  if (ns % years(1) == nanoseconds(0))
    return std::format("{}y", duration_cast<years>(ns).count());
  if (ns % months(1) == nanoseconds(0))
    return std::format("{}m", duration_cast<months>(ns).count());
  if (ns % days(1) == nanoseconds(0))
    return std::format("{}d", duration_cast<days>(ns).count());
  if (ns % hours(1) == nanoseconds(0))
    return std::format("{}h", duration_cast<hours>(ns).count());
  if (ns % minutes(1) == nanoseconds(0))
    return std::format("{}min", duration_cast<minutes>(ns).count());
  if (ns % seconds(1) == nanoseconds(0))
    return std::format("{}s", duration_cast<seconds>(ns).count());
  if (ns % milliseconds(1) == nanoseconds(0))
    return std::format("{}ms", duration_cast<milliseconds>(ns).count());
  if (ns % microseconds(1) == nanoseconds(0))
    return std::format("{}us", duration_cast<microseconds>(ns).count());

  return std::format("{}ns", ns.count());
}

template <typename T>
std::expected<T, std::string> ParseNumber(const std::string& str) {
  T value;
  auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), value);
  if (ec == std::errc()) {
    return value;
  }
  return std::unexpected(std::format("Failed to parse number: {}", str));
}

}  // namespace

std::expected<Config, std::string> ConfigManager::Load(
    const std::filesystem::path& path) {
  mINI::INIFile file(path.string());
  mINI::INIStructure ini;

  if (!file.read(ini)) {
    return std::unexpected(
        std::format("Failed to read config file: {}", path.string()));
  }

  Config config;

  // Helper macro to parse values
  auto parse_value = [&](const std::string& section, const std::string& key,
                         auto& target,
                         auto parser) -> std::optional<std::string> {
    if (ini.has(section) && ini[section].has(key)) {
      auto result = parser(ini[section][key]);
      if (result) {
        target = *result;
      } else {
        return std::format("Error parsing [{}] {}: {}", section, key,
                           result.error());
      }
    }
    return std::nullopt;
  };

  // Price
  if (auto err = parse_value("Price", "initial_price", config.initial_price,
                             ParseNumber<Price>))
    return std::unexpected(*err);
  if (auto err = parse_value("Price", "average_trend_value",
                             config.average_trend_value, ParseNumber<double>))
    return std::unexpected(*err);
  if (auto err = parse_value("Price", "price_variation", config.price_variation,
                             ParseNumber<double>))
    return std::unexpected(*err);
  if (auto err = parse_value("Price", "time_horizon", config.time_horizon,
                             ParseDuration))
    return std::unexpected(*err);
  if (auto err = parse_value("Price", "min_diff_time", config.min_diff_time,
                             ParseDuration))
    return std::unexpected(*err);
  if (auto err = parse_value("Price", "max_diff_time", config.max_diff_time,
                             ParseDuration))
    return std::unexpected(*err);

  // Trade
  if (auto err =
          parse_value("Trade", "fast_ema", config.fast_ema, ParseDuration))
    return std::unexpected(*err);
  if (auto err =
          parse_value("Trade", "slow_ema", config.slow_ema, ParseDuration))
    return std::unexpected(*err);
  if (auto err = parse_value("Trade", "min_volume", config.min_volume,
                             ParseNumber<Volume>))
    return std::unexpected(*err);
  if (auto err = parse_value("Trade", "max_volume", config.max_volume,
                             ParseNumber<Volume>))
    return std::unexpected(*err);
  if (auto err = parse_value("Trade", "min_position", config.min_position,
                             ParseNumber<Volume>))
    return std::unexpected(*err);
  if (auto err = parse_value("Trade", "max_position", config.max_position,
                             ParseNumber<Volume>))
    return std::unexpected(*err);

  // Exchange
  if (auto err = parse_value("Exchange", "rejection_probability",
                             config.rejection_probability, ParseNumber<double>))
    return std::unexpected(*err);

  // Simulation
  if (auto err = parse_value("Simulation", "steps_count", config.steps_count,
                             ParseNumber<uint64_t>))
    return std::unexpected(*err);

  if (ini.has("Simulation") && ini["Simulation"].has("price_evolution_path")) {
    config.price_evolution_path = ini["Simulation"]["price_evolution_path"];
  }
  if (ini.has("Simulation") && ini["Simulation"].has("orders_log_path")) {
    config.orders_log_path = ini["Simulation"]["orders_log_path"];
  }

  // Validation
  if (config.initial_price < 0)
    return std::unexpected("initial_price must be >= 0");
  if (config.time_horizon < std::chrono::nanoseconds(1))
    return std::unexpected("time_horizon must be >= 1ns");

  if (config.min_diff_time >= config.max_diff_time)
    return std::unexpected("min_diff_time must be < max_diff_time");
  if (config.min_diff_time < std::chrono::nanoseconds(1))
    return std::unexpected("min_diff_time must be >= 1ns");

  if (config.fast_ema < std::chrono::nanoseconds(1))
    return std::unexpected("fast_ema must be >= 1ns");
  if (config.slow_ema <= config.fast_ema)
    return std::unexpected("slow_ema must be > fast_ema");

  if (config.max_volume < config.min_volume)
    return std::unexpected("max_volume must be >= min_volume");
  if (config.min_volume < 0) return std::unexpected("min_volume must be >= 0");

  if (config.max_position < config.min_position)
    return std::unexpected("max_position must be >= min_position");

  if (config.rejection_probability < 0.0 ||
      config.rejection_probability > 100.0) {
    return std::unexpected(
        "rejection_probability must be between 0.0 and 100.0");
  }

  if (config.steps_count < 1)
    return std::unexpected("steps_count must be >= 1");

  return config;
}

std::expected<Config, std::string> ConfigManager::CreateDefaultConfig(
    const std::filesystem::path& path) {
  Config config;  // Default values
  mINI::INIFile file(path.string());
  mINI::INIStructure ini;

  ini["Price"]["initial_price"] = std::to_string(config.initial_price);
  ini["Price"]["average_trend_value"] =
      std::format("{}", config.average_trend_value);
  ini["Price"]["price_variation"] = std::format("{}", config.price_variation);
  ini["Price"]["time_horizon"] = DurationToString(config.time_horizon);
  ini["Price"]["min_diff_time"] = DurationToString(config.min_diff_time);
  ini["Price"]["max_diff_time"] = DurationToString(config.max_diff_time);

  ini["Trade"]["fast_ema"] = DurationToString(config.fast_ema);
  ini["Trade"]["slow_ema"] = DurationToString(config.slow_ema);
  ini["Trade"]["min_volume"] = std::to_string(config.min_volume);
  ini["Trade"]["max_volume"] = std::to_string(config.max_volume);
  ini["Trade"]["min_position"] = std::to_string(config.min_position);
  ini["Trade"]["max_position"] = std::to_string(config.max_position);

  ini["Exchange"]["rejection_probability"] =
      std::format("{}", config.rejection_probability);

  ini["Simulation"]["steps_count"] = std::to_string(config.steps_count);
  ini["Simulation"]["price_evolution_path"] =
      config.price_evolution_path.string();
  ini["Simulation"]["orders_log_path"] = config.orders_log_path.string();

  if (!file.generate(ini, true)) {
    return std::unexpected("Failed to write default config file");
  }

  return config;
}