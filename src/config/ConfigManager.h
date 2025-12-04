#ifndef TRADINGSIMULATOR_CONFIGMANAGER_H
#define TRADINGSIMULATOR_CONFIGMANAGER_H

#include <expected>
#include <filesystem>
#include <string>

#include "Config.h"

class ConfigManager {
 public:
  static std::expected<Config, std::string> Load(
      const std::filesystem::path& path);
  static std::expected<Config, std::string> CreateDefaultConfig(
      const std::filesystem::path& path);
};

#endif  // TRADINGSIMULATOR_CONFIGMANAGER_H
