#include <print>

#include "config/ConfigManager.h"
#include "simulation/Simulator.h"

std::filesystem::path GetExecutableDirectory(const char* argv0) {
  const std::filesystem::path exe_path(argv0);

  if (exe_path.is_absolute()) {
    return exe_path.parent_path();
  }

  return std::filesystem::absolute(exe_path).parent_path();
}

std::expected<Config, std::string> LoadOrCreateConfig(
    const std::filesystem::path& config_path) {
  std::error_code ec;
  bool file_exists = std::filesystem::exists(config_path, ec);

  if (ec) {
    return std::unexpected(std::format("Cannot access path '{}': {}",
                                       config_path.string(), ec.message()));
  }

  if (file_exists) {
    return ConfigManager::Load(config_path);
  }

  std::println("Configuration file not found, creating default: {}",
               config_path.string());
  return ConfigManager::CreateDefaultConfig(config_path);
}

[[noreturn]] void PrintUsageAndExit() {
  std::println("Usage: TradingSim [CONFIG_PATH]");
  std::println("");
  std::println("Arguments:");
  std::println("  CONFIG_PATH    Optional path to configuration file");
  std::println(
      "                 (default: config.ini in executable directory)");
  std::println("");
  std::println("Description:");
  std::println("  Runs a Geometric Brownian Motion trading simulation with");
  std::println("  moving average strategy.");
  std::println("");
  std::println("  If CONFIG_PATH is provided:");
  std::println("    - Loads configuration from the specified path");
  std::println("    - Creates default configuration if file doesn't exist");
  std::println("");
  std::println("  If CONFIG_PATH is omitted:");
  std::println("    - Uses 'config.ini' in executable directory");
  std::println("    - Creates default 'config.ini' if it doesn't exist");
  std::println("");
  std::println("Examples:");
  std::println("  TradingSim                     # Use default config.ini");
  std::println("  TradingSim my_config.ini       # Use custom configuration");
  std::println("  TradingSim C:\\configs\\sim.ini  # Use absolute path");

  exit(1);
}

int main(int argc, char* argv[]) {
  std::println("========================================");
  std::println("Trading Simulation - GBM with TimeMA Signals");
  std::println("========================================");
  std::println("");

  if (argc > 2) {
    std::println("Error: Too many arguments provided");
    std::println("");
    PrintUsageAndExit();
  }

  std::filesystem::path config_path;

  if (argc == 2) {
    config_path = argv[1];
  } else {
    config_path = GetExecutableDirectory(argv[0]) / "config.ini";
  }

  std::println("Using configuration file: {}", config_path.string());
  std::println("");

  auto config_result = LoadOrCreateConfig(config_path);

  if (!config_result) {
    std::println("Error: {}", config_result.error());
    return 1;
  }

  const Config config = config_result.value();
  Simulator simulator(config);
  simulator.Run();

  std::println("Simulation finished.");
  return 0;
}
