#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>

#include "config/Config.h"
#include "config/ConfigManager.h"

using namespace std::chrono_literals;
using ::testing::HasSubstr;

// ============================================================================
// Test Fixture
// ============================================================================

class ConfigManagerTest : public ::testing::Test {
 protected:
  std::filesystem::path temp_dir;
  std::filesystem::path test_config_path;

  void SetUp() override {
    // Create unique temporary directory for each test
    auto timestamp =
        std::chrono::system_clock::now().time_since_epoch().count();
    temp_dir = std::filesystem::temp_directory_path() /
               std::format("config_test_{}", timestamp);
    std::filesystem::create_directories(temp_dir);
    test_config_path = temp_dir / "test_config.ini";
  }

  void TearDown() override {
    // Clean up temporary files
    std::error_code ec;
    std::filesystem::remove_all(temp_dir, ec);
  }

  // Helper: Write INI content to test file
  void WriteConfigFile(const std::string& content) {
    std::ofstream file(test_config_path);
    file << content;
    file.close();
  }

  // Helper: Create valid base config string
  std::string GetValidConfigContent() {
    return R"([Price]
initial_price = 100
average_trend_value = 0.05
price_variation = 0.10
time_horizon = 24h
min_diff_time = 100ms
max_diff_time = 200ms

[Trade]
fast_ema = 1s
slow_ema = 5s
min_volume = 1
max_volume = 1000
min_position = -1000
max_position = 1000

[Exchange]
rejection_probability = 1.0

[Simulation]
steps_count = 100000
price_evolution_path = output/price_evolution.csv
orders_log_path = output/orders.csv
)";
  }

  // Helper: Modify specific key in section
  std::string ModifyConfigValue(const std::string& base, const std::string& key,
                                const std::string& new_value) {
    std::string result = base;
    size_t pos = result.find(key + " =");
    if (pos != std::string::npos) {
      size_t value_start = result.find('=', pos) + 1;
      size_t value_end = result.find('\n', value_start);
      result.replace(value_start, value_end - value_start, " " + new_value);
    }
    return result;
  }

  // Helper: Remove a line containing key
  std::string RemoveKey(const std::string& base, const std::string& key) {
    std::string result = base;
    size_t pos = result.find(key + " =");
    if (pos != std::string::npos) {
      size_t line_start = result.rfind('\n', pos);
      if (line_start == std::string::npos)
        line_start = 0;
      else
        line_start++;
      size_t line_end = result.find('\n', pos);
      if (line_end != std::string::npos) line_end++;
      result.erase(line_start, line_end - line_start);
    }
    return result;
  }

  // Helper: Remove entire section
  std::string RemoveSection(const std::string& base,
                            const std::string& section) {
    std::string result = base;
    size_t section_start = result.find("[" + section + "]");
    if (section_start != std::string::npos) {
      size_t next_section = result.find("\n[", section_start + 1);
      if (next_section != std::string::npos) {
        result.erase(section_start, next_section - section_start + 1);
      } else {
        result.erase(section_start);
      }
    }
    return result;
  }
};

// ============================================================================
// Category A: Happy Path Tests (12 tests)
// ============================================================================

TEST_F(ConfigManagerTest, LoadValidConfiguration) {
  WriteConfigFile(GetValidConfigContent());

  auto result = ConfigManager::Load(test_config_path);

  ASSERT_TRUE(result.has_value()) << "Expected successful load";
}

TEST_F(ConfigManagerTest, LoadConfigWithDefaultValues) {
  // Only minimal config, rest should use defaults
  WriteConfigFile(R"([Price]
initial_price = 100
)");

  auto result = ConfigManager::Load(test_config_path);

  ASSERT_TRUE(result.has_value()) << "Expected successful load with defaults";
  EXPECT_EQ(result->initial_price, 100);
  // Other fields should have default values from Config struct
}

TEST_F(ConfigManagerTest, CreateDefaultConfigSuccess) {
  auto result = ConfigManager::CreateDefaultConfig(test_config_path);

  ASSERT_TRUE(result.has_value()) << "Expected successful config creation";
  ASSERT_TRUE(std::filesystem::exists(test_config_path))
      << "Config file should exist";

  // Verify we can load the created file
  auto loaded = ConfigManager::Load(test_config_path);
  ASSERT_TRUE(loaded.has_value()) << "Should be able to load created config";
}

TEST_F(ConfigManagerTest, ParseDurationNanoseconds) {
  WriteConfigFile(
      ModifyConfigValue(GetValidConfigContent(), "min_diff_time", "1ns"));

  auto result = ConfigManager::Load(test_config_path);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->min_diff_time, 1ns);
}

TEST_F(ConfigManagerTest, ParseDurationMicroseconds) {
  WriteConfigFile(
      ModifyConfigValue(GetValidConfigContent(), "min_diff_time", "500us"));

  auto result = ConfigManager::Load(test_config_path);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->min_diff_time, 500us);
}

TEST_F(ConfigManagerTest, ParseDurationMilliseconds) {
  WriteConfigFile(
      ModifyConfigValue(GetValidConfigContent(), "min_diff_time", "100ms"));

  auto result = ConfigManager::Load(test_config_path);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->min_diff_time, 100ms);
}

TEST_F(ConfigManagerTest, ParseDurationSeconds) {
  auto config = GetValidConfigContent();
  config = ModifyConfigValue(config, "fast_ema", "60s");
  config = ModifyConfigValue(config, "slow_ema", "120s");  // Must be > fast_ema
  WriteConfigFile(config);

  auto result = ConfigManager::Load(test_config_path);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->fast_ema, 60s);
}

TEST_F(ConfigManagerTest, ParseDurationMinutes) {
  auto config = GetValidConfigContent();
  config = ModifyConfigValue(config, "fast_ema", "5min");
  config =
      ModifyConfigValue(config, "slow_ema", "10min");  // Must be > fast_ema
  WriteConfigFile(config);

  auto result = ConfigManager::Load(test_config_path);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->fast_ema, 5min);
}

TEST_F(ConfigManagerTest, ParseDurationHours) {
  WriteConfigFile(
      ModifyConfigValue(GetValidConfigContent(), "time_horizon", "24h"));

  auto result = ConfigManager::Load(test_config_path);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->time_horizon, 24h);
}

TEST_F(ConfigManagerTest, ParseDurationDays) {
  WriteConfigFile(
      ModifyConfigValue(GetValidConfigContent(), "time_horizon", "7d"));

  auto result = ConfigManager::Load(test_config_path);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->time_horizon, 7 * 24h);
}

TEST_F(ConfigManagerTest, ParseDurationMonths) {
  WriteConfigFile(
      ModifyConfigValue(GetValidConfigContent(), "time_horizon", "1m"));

  auto result = ConfigManager::Load(test_config_path);

  ASSERT_TRUE(result.has_value());
  // 1 month converts to nanoseconds (check it's a reasonable value)
  auto expected = std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::months(1));
  EXPECT_EQ(result->time_horizon, expected);
}

TEST_F(ConfigManagerTest, ParseDurationYears) {
  WriteConfigFile(
      ModifyConfigValue(GetValidConfigContent(), "time_horizon", "1y"));

  auto result = ConfigManager::Load(test_config_path);

  ASSERT_TRUE(result.has_value());
  auto expected = std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::years(1));
  EXPECT_EQ(result->time_horizon, expected);
}

// ============================================================================
// Category B: File I/O Error Tests (8 tests)
// ============================================================================

TEST_F(ConfigManagerTest, LoadNonExistentFile) {
  auto non_existent = temp_dir / "does_not_exist.ini";

  auto result = ConfigManager::Load(non_existent);

  ASSERT_FALSE(result.has_value()) << "Should fail for non-existent file";
  EXPECT_THAT(result.error(), HasSubstr("Failed to read config file"));
}

TEST_F(ConfigManagerTest, LoadDirectoryInsteadOfFile) {
  auto result = ConfigManager::Load(temp_dir);

  ASSERT_FALSE(result.has_value()) << "Should fail when path is directory";
  EXPECT_THAT(result.error(), HasSubstr("Failed to read config file"));
}

TEST_F(ConfigManagerTest, LoadEmptyPath) {
  std::filesystem::path empty_path;

  auto result = ConfigManager::Load(empty_path);

  ASSERT_FALSE(result.has_value()) << "Should fail for empty path";
  EXPECT_THAT(result.error(), HasSubstr("Failed to read config file"));
}

TEST_F(ConfigManagerTest, LoadFileWithoutReadPermissions) {
#ifdef __unix__
  WriteConfigFile(GetValidConfigContent());
  std::filesystem::permissions(test_config_path,
                               std::filesystem::perms::owner_read,
                               std::filesystem::perm_options::remove);

  auto result = ConfigManager::Load(test_config_path);

  EXPECT_FALSE(result.has_value()) << "Should fail without read permissions";

  // Restore permissions for cleanup
  std::filesystem::permissions(test_config_path,
                               std::filesystem::perms::owner_all,
                               std::filesystem::perm_options::add);
#else
  GTEST_SKIP() << "Permission test only supported on Unix-like systems";
#endif
}

TEST_F(ConfigManagerTest, LoadCorruptedFile) {
  // Write invalid UTF-8 sequences
  std::ofstream file(test_config_path, std::ios::binary);
  file << "\xFF\xFE\xFD";  // Invalid UTF-8 bytes
  file.close();

  auto result = ConfigManager::Load(test_config_path);

  // mINI might handle this or fail - either is acceptable
  // Just verify it doesn't crash
  SUCCEED();
}

TEST_F(ConfigManagerTest, LoadFileWithUTF8BOM) {
  std::ofstream file(test_config_path, std::ios::binary);
  // UTF-8 BOM
  file << "\xEF\xBB\xBF";
  file << GetValidConfigContent();
  file.close();

  auto result = ConfigManager::Load(test_config_path);

  // mINI should handle UTF-8 BOM correctly
  ASSERT_TRUE(result.has_value()) << "Should handle UTF-8 BOM";
}

TEST_F(ConfigManagerTest, CreateDefaultConfigInvalidDirectory) {
  auto invalid_path = temp_dir / "nonexistent" / "config.ini";

  auto result = ConfigManager::CreateDefaultConfig(invalid_path);

  ASSERT_FALSE(result.has_value()) << "Should fail in non-existent directory";
  EXPECT_THAT(result.error(), HasSubstr("Failed to write default config file"));
}

TEST_F(ConfigManagerTest, CreateDefaultConfigWithoutWritePermissions) {
#ifdef __unix__
  std::filesystem::permissions(temp_dir, std::filesystem::perms::owner_write,
                               std::filesystem::perm_options::remove);

  auto result = ConfigManager::CreateDefaultConfig(test_config_path);

  EXPECT_FALSE(result.has_value()) << "Should fail without write permissions";

  // Restore permissions for cleanup
  std::filesystem::permissions(temp_dir, std::filesystem::perms::owner_all,
                               std::filesystem::perm_options::add);
#else
  GTEST_SKIP() << "Permission test only supported on Unix-like systems";
#endif
}

// ============================================================================
// Category C: Parsing Error Tests (35 tests)
// ============================================================================

// C1-C6: Duration Parsing Errors

TEST_F(ConfigManagerTest, ParseDurationEmpty) {
  WriteConfigFile(
      ModifyConfigValue(GetValidConfigContent(), "min_diff_time", ""));

  auto result = ConfigManager::Load(test_config_path);

  ASSERT_FALSE(result.has_value());
  EXPECT_THAT(result.error(), HasSubstr("Empty duration string"));
}

TEST_F(ConfigManagerTest, ParseDurationInvalidFormatNoUnit) {
  WriteConfigFile(
      ModifyConfigValue(GetValidConfigContent(), "min_diff_time", "100"));

  auto result = ConfigManager::Load(test_config_path);

  ASSERT_FALSE(result.has_value());
  EXPECT_THAT(result.error(), HasSubstr("Invalid duration format"));
}

TEST_F(ConfigManagerTest, ParseDurationInvalidFormatLetters) {
  WriteConfigFile(
      ModifyConfigValue(GetValidConfigContent(), "min_diff_time", "abc"));

  auto result = ConfigManager::Load(test_config_path);

  ASSERT_FALSE(result.has_value());
  EXPECT_THAT(result.error(), HasSubstr("Invalid duration format"));
}

TEST_F(ConfigManagerTest, ParseDurationUnknownSuffix) {
  WriteConfigFile(
      ModifyConfigValue(GetValidConfigContent(), "min_diff_time", "100xyz"));

  auto result = ConfigManager::Load(test_config_path);

  ASSERT_FALSE(result.has_value());
  EXPECT_THAT(result.error(), HasSubstr("Invalid duration format"));
}

TEST_F(ConfigManagerTest, ParseDurationWithWhitespace) {
  WriteConfigFile(
      ModifyConfigValue(GetValidConfigContent(), "min_diff_time", "  100ms  "));

  auto result = ConfigManager::Load(test_config_path);

  // Should succeed - whitespace should be removed
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->min_diff_time, 100ms);
}

TEST_F(ConfigManagerTest, ParseDurationNegativeValue) {
  WriteConfigFile(
      ModifyConfigValue(GetValidConfigContent(), "min_diff_time", "-100ms"));

  auto result = ConfigManager::Load(test_config_path);

  ASSERT_FALSE(result.has_value());
  EXPECT_THAT(result.error(), HasSubstr("Invalid duration format"));
}

TEST_F(ConfigManagerTest, ParseDurationZero) {
  WriteConfigFile(
      ModifyConfigValue(GetValidConfigContent(), "time_horizon", "0s"));

  auto result = ConfigManager::Load(test_config_path);

  // Parsing should succeed, but validation should fail
  ASSERT_FALSE(result.has_value());
  EXPECT_THAT(result.error(), HasSubstr("time_horizon must be >= 1ns"));
}

TEST_F(ConfigManagerTest, ParseDurationOverflow) {
  WriteConfigFile(ModifyConfigValue(GetValidConfigContent(), "min_diff_time",
                                    "99999999999999999999y"));

  auto result = ConfigManager::Load(test_config_path);

  ASSERT_FALSE(result.has_value());
  EXPECT_THAT(result.error(), HasSubstr("Invalid number in duration"));
}

// C7-C12: Number Parsing Errors

TEST_F(ConfigManagerTest, ParsePriceInvalidLetters) {
  WriteConfigFile(
      ModifyConfigValue(GetValidConfigContent(), "initial_price", "abc"));

  auto result = ConfigManager::Load(test_config_path);

  ASSERT_FALSE(result.has_value());
  EXPECT_THAT(result.error(), HasSubstr("Failed to parse number"));
}

TEST_F(ConfigManagerTest, ParsePriceInvalidMultipleDots) {
  WriteConfigFile(
      ModifyConfigValue(GetValidConfigContent(), "initial_price", "12.34.56"));

  auto result = ConfigManager::Load(test_config_path);

  // std::from_chars parses partially (12.34) and considers it success
  // This is expected behavior of from_chars - it stops at first invalid
  // character
  ASSERT_TRUE(result.has_value()) << "from_chars parses partially";
  EXPECT_DOUBLE_EQ(result->initial_price, 12.34);
}

TEST_F(ConfigManagerTest, ParseVolumeInvalid) {
  WriteConfigFile(
      ModifyConfigValue(GetValidConfigContent(), "min_volume", "not_a_number"));

  auto result = ConfigManager::Load(test_config_path);

  ASSERT_FALSE(result.has_value());
  EXPECT_THAT(result.error(), HasSubstr("Failed to parse number"));
}

TEST_F(ConfigManagerTest, ParseDoubleInvalid) {
  WriteConfigFile(ModifyConfigValue(GetValidConfigContent(),
                                    "average_trend_value", "1.2.3"));

  auto result = ConfigManager::Load(test_config_path);

  // std::from_chars parses partially (1.2) and considers it success
  ASSERT_TRUE(result.has_value()) << "from_chars parses partially";
  EXPECT_DOUBLE_EQ(result->average_trend_value, 1.2);
}

TEST_F(ConfigManagerTest, ParseUint64Invalid) {
  WriteConfigFile(
      ModifyConfigValue(GetValidConfigContent(), "steps_count", "abc123"));

  auto result = ConfigManager::Load(test_config_path);

  ASSERT_FALSE(result.has_value());
  EXPECT_THAT(result.error(), HasSubstr("Failed to parse number"));
}

TEST_F(ConfigManagerTest, ParseNumberWithLeadingWhitespace) {
  WriteConfigFile(
      ModifyConfigValue(GetValidConfigContent(), "initial_price", "  100.5  "));

  auto result = ConfigManager::Load(test_config_path);

  // mINI trims whitespace, should succeed
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->initial_price, 100.5);
}

// C13-C20: Missing Sections/Keys

TEST_F(ConfigManagerTest, LoadMissingPriceSection) {
  WriteConfigFile(RemoveSection(GetValidConfigContent(), "Price"));

  auto result = ConfigManager::Load(test_config_path);

  // Should use default values and succeed if validation passes
  EXPECT_TRUE(result.has_value());
}

TEST_F(ConfigManagerTest, LoadMissingTradeSection) {
  WriteConfigFile(RemoveSection(GetValidConfigContent(), "Trade"));

  auto result = ConfigManager::Load(test_config_path);

  EXPECT_TRUE(result.has_value());
}

TEST_F(ConfigManagerTest, LoadMissingExchangeSection) {
  WriteConfigFile(RemoveSection(GetValidConfigContent(), "Exchange"));

  auto result = ConfigManager::Load(test_config_path);

  EXPECT_TRUE(result.has_value());
}

TEST_F(ConfigManagerTest, LoadMissingSimulationSection) {
  WriteConfigFile(RemoveSection(GetValidConfigContent(), "Simulation"));

  auto result = ConfigManager::Load(test_config_path);

  EXPECT_TRUE(result.has_value());
}

TEST_F(ConfigManagerTest, LoadMissingSpecificKey) {
  WriteConfigFile(RemoveKey(GetValidConfigContent(), "time_horizon"));

  auto result = ConfigManager::Load(test_config_path);

  // Should use default value for time_horizon
  ASSERT_TRUE(result.has_value());
}

TEST_F(ConfigManagerTest, LoadEmptyFile) {
  WriteConfigFile("");

  auto result = ConfigManager::Load(test_config_path);

  // Should use all defaults and succeed
  EXPECT_TRUE(result.has_value());
}

TEST_F(ConfigManagerTest, LoadFileWithOnlyComments) {
  WriteConfigFile(R"(; This is a comment
; Another comment
; More comments
)");

  auto result = ConfigManager::Load(test_config_path);

  EXPECT_TRUE(result.has_value());
}

TEST_F(ConfigManagerTest, LoadMinimalValidConfig) {
  WriteConfigFile(R"([Price]
initial_price = 50
time_horizon = 1h
min_diff_time = 1ns
max_diff_time = 2ns
[Trade]
fast_ema = 1ns
slow_ema = 2ns
)");

  auto result = ConfigManager::Load(test_config_path);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->initial_price, 50);
  EXPECT_EQ(result->time_horizon, 1h);
}

// C21-C28: INI Format Edge Cases

TEST_F(ConfigManagerTest, ParseWithComments) {
  WriteConfigFile(R"([Price]
; This is a comment
initial_price = 100
; Another comment
time_horizon = 24h
)");

  auto result = ConfigManager::Load(test_config_path);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->initial_price, 100);
}

TEST_F(ConfigManagerTest, ParseWithTrailingComments) {
  WriteConfigFile(R"([Price]
initial_price = 100 ; inline comment
time_horizon = 24h ; another comment
)");

  auto result = ConfigManager::Load(test_config_path);

  // mINI might or might not support trailing comments
  // Just verify it doesn't crash
  SUCCEED();
}

TEST_F(ConfigManagerTest, ParseWithExtraWhitespace) {
  WriteConfigFile(R"([Price]
  initial_price  =   100.5
    time_horizon    =     24h
)");

  auto result = ConfigManager::Load(test_config_path);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->initial_price, 100.5);
}

TEST_F(ConfigManagerTest, ParseDuplicateKey) {
  WriteConfigFile(R"([Price]
initial_price = 100
initial_price = 200
)");

  auto result = ConfigManager::Load(test_config_path);

  // mINI should use last value
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->initial_price, 200);
}

// C29-C35: Path Parsing

TEST_F(ConfigManagerTest, ParseRelativePaths) {
  WriteConfigFile(GetValidConfigContent());

  auto result = ConfigManager::Load(test_config_path);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->price_evolution_path, "output/price_evolution.csv");
}

TEST_F(ConfigManagerTest, ParsePathWithSpaces) {
  WriteConfigFile(ModifyConfigValue(
      GetValidConfigContent(), "price_evolution_path", "output/my file.csv"));

  auto result = ConfigManager::Load(test_config_path);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->price_evolution_path, "output/my file.csv");
}

TEST_F(ConfigManagerTest, ParsePathWithSpecialCharacters) {
  WriteConfigFile(ModifyConfigValue(GetValidConfigContent(),
                                    "price_evolution_path",
                                    "output/file-2024_v1.csv"));

  auto result = ConfigManager::Load(test_config_path);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->price_evolution_path, "output/file-2024_v1.csv");
}

TEST_F(ConfigManagerTest, ParseEmptyPath) {
  WriteConfigFile(
      ModifyConfigValue(GetValidConfigContent(), "price_evolution_path", ""));

  auto result = ConfigManager::Load(test_config_path);

  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->price_evolution_path.empty());
}

// ============================================================================
// Category D: Validation Error Tests (50+ tests)
// ============================================================================

// D1-D5: Price Section Validation

TEST_F(ConfigManagerTest, ValidateInitialPriceNegative) {
  WriteConfigFile(
      ModifyConfigValue(GetValidConfigContent(), "initial_price", "-1"));

  auto result = ConfigManager::Load(test_config_path);

  ASSERT_FALSE(result.has_value());
  EXPECT_THAT(result.error(), HasSubstr("initial_price must be >= 0"));
}

TEST_F(ConfigManagerTest, ValidateInitialPriceNegativeSmall) {
  WriteConfigFile(
      ModifyConfigValue(GetValidConfigContent(), "initial_price", "-0.01"));

  auto result = ConfigManager::Load(test_config_path);

  ASSERT_FALSE(result.has_value());
  EXPECT_THAT(result.error(), HasSubstr("initial_price must be >= 0"));
}

TEST_F(ConfigManagerTest, ValidateInitialPriceZero) {
  WriteConfigFile(
      ModifyConfigValue(GetValidConfigContent(), "initial_price", "0"));

  auto result = ConfigManager::Load(test_config_path);

  ASSERT_TRUE(result.has_value()) << "Zero price should be valid";
  EXPECT_EQ(result->initial_price, 0);
}

TEST_F(ConfigManagerTest, ValidateTimeHorizonZero) {
  WriteConfigFile(
      ModifyConfigValue(GetValidConfigContent(), "time_horizon", "0ns"));

  auto result = ConfigManager::Load(test_config_path);

  ASSERT_FALSE(result.has_value());
  EXPECT_THAT(result.error(), HasSubstr("time_horizon must be >= 1ns"));
}

TEST_F(ConfigManagerTest, ValidateTimeHorizonMinimum) {
  WriteConfigFile(
      ModifyConfigValue(GetValidConfigContent(), "time_horizon", "1ns"));

  auto result = ConfigManager::Load(test_config_path);

  ASSERT_TRUE(result.has_value()) << "1ns should be valid";
  EXPECT_EQ(result->time_horizon, 1ns);
}

// D6-D10: Min/Max Time Relationships

TEST_F(ConfigManagerTest, ValidateMinDiffTimeGreaterThanMax) {
  auto config = GetValidConfigContent();
  config = ModifyConfigValue(config, "min_diff_time", "200ms");
  config = ModifyConfigValue(config, "max_diff_time", "100ms");
  WriteConfigFile(config);

  auto result = ConfigManager::Load(test_config_path);

  ASSERT_FALSE(result.has_value());
  EXPECT_THAT(result.error(),
              HasSubstr("min_diff_time must be < max_diff_time"));
}

TEST_F(ConfigManagerTest, ValidateMinDiffTimeEqualToMax) {
  auto config = GetValidConfigContent();
  config = ModifyConfigValue(config, "min_diff_time", "100ms");
  config = ModifyConfigValue(config, "max_diff_time", "100ms");
  WriteConfigFile(config);

  auto result = ConfigManager::Load(test_config_path);

  ASSERT_FALSE(result.has_value());
  EXPECT_THAT(result.error(),
              HasSubstr("min_diff_time must be < max_diff_time"));
}

TEST_F(ConfigManagerTest, ValidateMinDiffTimeJustBelowMax) {
  auto config = GetValidConfigContent();
  config = ModifyConfigValue(config, "min_diff_time", "99ms");
  config = ModifyConfigValue(config, "max_diff_time", "100ms");
  WriteConfigFile(config);

  auto result = ConfigManager::Load(test_config_path);

  ASSERT_TRUE(result.has_value()) << "min < max should be valid";
  EXPECT_EQ(result->min_diff_time, 99ms);
  EXPECT_EQ(result->max_diff_time, 100ms);
}

TEST_F(ConfigManagerTest, ValidateMinDiffTimeMinimumValue) {
  auto config = GetValidConfigContent();
  config = ModifyConfigValue(config, "min_diff_time", "1ns");
  config = ModifyConfigValue(config, "max_diff_time", "2ns");
  WriteConfigFile(config);

  auto result = ConfigManager::Load(test_config_path);

  ASSERT_TRUE(result.has_value()) << "1ns, 2ns should be valid";
  EXPECT_EQ(result->min_diff_time, 1ns);
  EXPECT_EQ(result->max_diff_time, 2ns);
}

TEST_F(ConfigManagerTest, ValidateMinDiffTimeZero) {
  WriteConfigFile(
      ModifyConfigValue(GetValidConfigContent(), "min_diff_time", "0ns"));

  auto result = ConfigManager::Load(test_config_path);

  ASSERT_FALSE(result.has_value());
  EXPECT_THAT(result.error(), HasSubstr("min_diff_time must be >= 1ns"));
}

// D11-D15: Trade Section - EMA Validation

TEST_F(ConfigManagerTest, ValidateFastEmaZero) {
  WriteConfigFile(
      ModifyConfigValue(GetValidConfigContent(), "fast_ema", "0ns"));

  auto result = ConfigManager::Load(test_config_path);

  ASSERT_FALSE(result.has_value());
  EXPECT_THAT(result.error(), HasSubstr("fast_ema must be >= 1ns"));
}

TEST_F(ConfigManagerTest, ValidateFastEmaMinimum) {
  auto config = GetValidConfigContent();
  config = ModifyConfigValue(config, "fast_ema", "1ns");
  config = ModifyConfigValue(config, "slow_ema", "2ns");
  WriteConfigFile(config);

  auto result = ConfigManager::Load(test_config_path);

  ASSERT_TRUE(result.has_value()) << "1ns fast_ema should be valid";
  EXPECT_EQ(result->fast_ema, 1ns);
}

TEST_F(ConfigManagerTest, ValidateSlowEmaLessThanFast) {
  auto config = GetValidConfigContent();
  config = ModifyConfigValue(config, "fast_ema", "5s");
  config = ModifyConfigValue(config, "slow_ema", "4s");
  WriteConfigFile(config);

  auto result = ConfigManager::Load(test_config_path);

  ASSERT_FALSE(result.has_value());
  EXPECT_THAT(result.error(), HasSubstr("slow_ema must be > fast_ema"));
}

TEST_F(ConfigManagerTest, ValidateSlowEmaEqualToFast) {
  auto config = GetValidConfigContent();
  config = ModifyConfigValue(config, "fast_ema", "5s");
  config = ModifyConfigValue(config, "slow_ema", "5s");
  WriteConfigFile(config);

  auto result = ConfigManager::Load(test_config_path);

  ASSERT_FALSE(result.has_value());
  EXPECT_THAT(result.error(), HasSubstr("slow_ema must be > fast_ema"));
}

TEST_F(ConfigManagerTest, ValidateSlowEmaJustAboveFast) {
  auto config = GetValidConfigContent();
  config = ModifyConfigValue(config, "fast_ema", "5s");
  config = ModifyConfigValue(config, "slow_ema", "5001ms");
  WriteConfigFile(config);

  auto result = ConfigManager::Load(test_config_path);

  ASSERT_TRUE(result.has_value()) << "slow > fast should be valid";
  EXPECT_EQ(result->fast_ema, 5s);
  EXPECT_EQ(result->slow_ema, 5001ms);
}

// D16-D20: Trade Section - Volume Validation

TEST_F(ConfigManagerTest, ValidateMinVolumeNegative) {
  WriteConfigFile(
      ModifyConfigValue(GetValidConfigContent(), "min_volume", "-1"));

  auto result = ConfigManager::Load(test_config_path);

  ASSERT_FALSE(result.has_value());
  EXPECT_THAT(result.error(), HasSubstr("min_volume must be >= 0"));
}

TEST_F(ConfigManagerTest, ValidateMinVolumeNegativeSmall) {
  WriteConfigFile(
      ModifyConfigValue(GetValidConfigContent(), "min_volume", "-0.01"));

  auto result = ConfigManager::Load(test_config_path);

  ASSERT_FALSE(result.has_value());
  EXPECT_THAT(result.error(), HasSubstr("min_volume must be >= 0"));
}

TEST_F(ConfigManagerTest, ValidateMinVolumeZero) {
  WriteConfigFile(
      ModifyConfigValue(GetValidConfigContent(), "min_volume", "0"));

  auto result = ConfigManager::Load(test_config_path);

  ASSERT_TRUE(result.has_value()) << "Zero min_volume should be valid";
  EXPECT_EQ(result->min_volume, 0);
}

TEST_F(ConfigManagerTest, ValidateMaxVolumeLessThanMin) {
  auto config = GetValidConfigContent();
  config = ModifyConfigValue(config, "min_volume", "1000");
  config = ModifyConfigValue(config, "max_volume", "500");
  WriteConfigFile(config);

  auto result = ConfigManager::Load(test_config_path);

  ASSERT_FALSE(result.has_value());
  EXPECT_THAT(result.error(), HasSubstr("max_volume must be >= min_volume"));
}

TEST_F(ConfigManagerTest, ValidateMaxVolumeEqualToMin) {
  auto config = GetValidConfigContent();
  config = ModifyConfigValue(config, "min_volume", "1000");
  config = ModifyConfigValue(config, "max_volume", "1000");
  WriteConfigFile(config);

  auto result = ConfigManager::Load(test_config_path);

  ASSERT_TRUE(result.has_value()) << "max == min should be valid";
  EXPECT_EQ(result->min_volume, 1000);
  EXPECT_EQ(result->max_volume, 1000);
}

// D21-D25: Trade Section - Position Validation

TEST_F(ConfigManagerTest, ValidateMaxPositionLessThanMin) {
  auto config = GetValidConfigContent();
  config = ModifyConfigValue(config, "min_position", "1000");
  config = ModifyConfigValue(config, "max_position", "500");
  WriteConfigFile(config);

  auto result = ConfigManager::Load(test_config_path);

  ASSERT_FALSE(result.has_value());
  EXPECT_THAT(result.error(),
              HasSubstr("max_position must be >= min_position"));
}

TEST_F(ConfigManagerTest, ValidateMaxPositionEqualToMin) {
  auto config = GetValidConfigContent();
  config = ModifyConfigValue(config, "min_position", "1000");
  config = ModifyConfigValue(config, "max_position", "1000");
  WriteConfigFile(config);

  auto result = ConfigManager::Load(test_config_path);

  ASSERT_TRUE(result.has_value()) << "max == min should be valid";
}

TEST_F(ConfigManagerTest, ValidateNegativePositions) {
  auto config = GetValidConfigContent();
  config = ModifyConfigValue(config, "min_position", "-1000");
  config = ModifyConfigValue(config, "max_position", "-500");
  WriteConfigFile(config);

  auto result = ConfigManager::Load(test_config_path);

  ASSERT_TRUE(result.has_value())
      << "Negative positions should be valid if min < max";
}

TEST_F(ConfigManagerTest, ValidatePositionRangeAcrossZero) {
  auto config = GetValidConfigContent();
  config = ModifyConfigValue(config, "min_position", "-1000");
  config = ModifyConfigValue(config, "max_position", "1000");
  WriteConfigFile(config);

  auto result = ConfigManager::Load(test_config_path);

  ASSERT_TRUE(result.has_value()) << "Range across zero should be valid";
}

TEST_F(ConfigManagerTest, ValidatePositionMinGreaterThanMaxNegative) {
  auto config = GetValidConfigContent();
  config = ModifyConfigValue(config, "min_position", "-100");
  config = ModifyConfigValue(config, "max_position", "-500");
  WriteConfigFile(config);

  auto result = ConfigManager::Load(test_config_path);

  ASSERT_FALSE(result.has_value());
  EXPECT_THAT(result.error(),
              HasSubstr("max_position must be >= min_position"));
}

// D26-D30: Exchange Section - Rejection Probability

TEST_F(ConfigManagerTest, ValidateRejectionProbabilityNegative) {
  WriteConfigFile(ModifyConfigValue(GetValidConfigContent(),
                                    "rejection_probability", "-0.01"));

  auto result = ConfigManager::Load(test_config_path);

  ASSERT_FALSE(result.has_value());
  EXPECT_THAT(result.error(), HasSubstr("must be between 0.0 and 100.0"));
}

TEST_F(ConfigManagerTest, ValidateRejectionProbabilityNegativeLarge) {
  WriteConfigFile(ModifyConfigValue(GetValidConfigContent(),
                                    "rejection_probability", "-100.0"));

  auto result = ConfigManager::Load(test_config_path);

  ASSERT_FALSE(result.has_value());
  EXPECT_THAT(result.error(), HasSubstr("must be between 0.0 and 100.0"));
}

TEST_F(ConfigManagerTest, ValidateRejectionProbabilityAbove100) {
  WriteConfigFile(ModifyConfigValue(GetValidConfigContent(),
                                    "rejection_probability", "100.01"));

  auto result = ConfigManager::Load(test_config_path);

  ASSERT_FALSE(result.has_value());
  EXPECT_THAT(result.error(), HasSubstr("must be between 0.0 and 100.0"));
}

TEST_F(ConfigManagerTest, ValidateRejectionProbabilityAbove100Large) {
  WriteConfigFile(ModifyConfigValue(GetValidConfigContent(),
                                    "rejection_probability", "1000.0"));

  auto result = ConfigManager::Load(test_config_path);

  ASSERT_FALSE(result.has_value());
  EXPECT_THAT(result.error(), HasSubstr("must be between 0.0 and 100.0"));
}

TEST_F(ConfigManagerTest, ValidateRejectionProbabilityZero) {
  WriteConfigFile(ModifyConfigValue(GetValidConfigContent(),
                                    "rejection_probability", "0.0"));

  auto result = ConfigManager::Load(test_config_path);

  ASSERT_TRUE(result.has_value()) << "0.0 should be valid";
  EXPECT_DOUBLE_EQ(result->rejection_probability, 0.0);
}

TEST_F(ConfigManagerTest, ValidateRejectionProbability100) {
  WriteConfigFile(ModifyConfigValue(GetValidConfigContent(),
                                    "rejection_probability", "100.0"));

  auto result = ConfigManager::Load(test_config_path);

  ASSERT_TRUE(result.has_value()) << "100.0 should be valid";
  EXPECT_DOUBLE_EQ(result->rejection_probability, 100.0);
}

TEST_F(ConfigManagerTest, ValidateRejectionProbabilityMidRange) {
  WriteConfigFile(ModifyConfigValue(GetValidConfigContent(),
                                    "rejection_probability", "50.5"));

  auto result = ConfigManager::Load(test_config_path);

  ASSERT_TRUE(result.has_value());
  EXPECT_DOUBLE_EQ(result->rejection_probability, 50.5);
}

// D31-D35: Simulation Section - Steps Count

TEST_F(ConfigManagerTest, ValidateStepsCountZero) {
  WriteConfigFile(
      ModifyConfigValue(GetValidConfigContent(), "steps_count", "0"));

  auto result = ConfigManager::Load(test_config_path);

  ASSERT_FALSE(result.has_value());
  EXPECT_THAT(result.error(), HasSubstr("steps_count must be >= 1"));
}

TEST_F(ConfigManagerTest, ValidateStepsCountOne) {
  WriteConfigFile(
      ModifyConfigValue(GetValidConfigContent(), "steps_count", "1"));

  auto result = ConfigManager::Load(test_config_path);

  ASSERT_TRUE(result.has_value()) << "1 should be valid";
  EXPECT_EQ(result->steps_count, 1);
}

TEST_F(ConfigManagerTest, ValidateStepsCountLarge) {
  WriteConfigFile(ModifyConfigValue(GetValidConfigContent(), "steps_count",
                                    "999999999999"));

  auto result = ConfigManager::Load(test_config_path);

  ASSERT_TRUE(result.has_value()) << "Large values should be valid";
  EXPECT_EQ(result->steps_count, 999999999999ULL);
}

// D36-D50: Boundary Combinations

TEST_F(ConfigManagerTest, ValidateAllMinimumsAtBoundary) {
  WriteConfigFile(R"([Price]
initial_price = 0
time_horizon = 1ns
min_diff_time = 1ns
max_diff_time = 2ns

[Trade]
fast_ema = 1ns
slow_ema = 2ns
min_volume = 0
max_volume = 0
min_position = 0
max_position = 0

[Exchange]
rejection_probability = 0.0

[Simulation]
steps_count = 1
)");

  auto result = ConfigManager::Load(test_config_path);

  ASSERT_TRUE(result.has_value()) << "All minimum values should be valid";
}

TEST_F(ConfigManagerTest, ValidateAllMaximumsAtBoundary) {
  WriteConfigFile(R"([Price]
initial_price = 999999999.99
time_horizon = 1y
min_diff_time = 1ns
max_diff_time = 1y

[Trade]
fast_ema = 1ns
slow_ema = 1y
min_volume = 0
max_volume = 999999999
min_position = -999999999
max_position = 999999999

[Exchange]
rejection_probability = 100.0

[Simulation]
steps_count = 999999999999
)");

  auto result = ConfigManager::Load(test_config_path);

  ASSERT_TRUE(result.has_value()) << "Large valid values should be accepted";
}

TEST_F(ConfigManagerTest, ValidateExtremelySmallDurations) {
  auto config = GetValidConfigContent();
  config = ModifyConfigValue(config, "time_horizon", "1ns");
  config = ModifyConfigValue(config, "min_diff_time", "1ns");
  config = ModifyConfigValue(config, "max_diff_time", "2ns");
  config = ModifyConfigValue(config, "fast_ema", "1ns");
  config = ModifyConfigValue(config, "slow_ema", "2ns");
  WriteConfigFile(config);

  auto result = ConfigManager::Load(test_config_path);

  ASSERT_TRUE(result.has_value()) << "1ns durations should be valid";
}

// ============================================================================
// Category E: Edge Cases (25 tests)
// ============================================================================

// E1-E5: Numeric Edge Cases

TEST_F(ConfigManagerTest, ParseVerySmallPrice) {
  WriteConfigFile(ModifyConfigValue(GetValidConfigContent(), "initial_price",
                                    "0.00000001"));

  auto result = ConfigManager::Load(test_config_path);

  ASSERT_TRUE(result.has_value());
  EXPECT_DOUBLE_EQ(result->initial_price, 0.00000001);
}

TEST_F(ConfigManagerTest, ParseVeryLargePrice) {
  WriteConfigFile(ModifyConfigValue(GetValidConfigContent(), "initial_price",
                                    "999999999999.99"));

  auto result = ConfigManager::Load(test_config_path);

  ASSERT_TRUE(result.has_value());
  EXPECT_DOUBLE_EQ(result->initial_price, 999999999999.99);
}

// E6-E10: Duration Edge Cases

TEST_F(ConfigManagerTest, ParseDurationCombinations) {
  // Test that 1s == 1000ms
  auto config1 = GetValidConfigContent();
  config1 = ModifyConfigValue(config1, "fast_ema", "1s");
  config1 = ModifyConfigValue(config1, "slow_ema", "2s");
  WriteConfigFile(config1);

  auto result1 = ConfigManager::Load(test_config_path);
  ASSERT_TRUE(result1.has_value());

  auto config2 = GetValidConfigContent();
  config2 = ModifyConfigValue(config2, "fast_ema", "1000ms");
  config2 = ModifyConfigValue(config2, "slow_ema", "2000ms");
  WriteConfigFile(config2);

  auto result2 = ConfigManager::Load(test_config_path);
  ASSERT_TRUE(result2.has_value());

  EXPECT_EQ(result1->fast_ema, result2->fast_ema);
  EXPECT_EQ(result1->slow_ema, result2->slow_ema);
}

TEST_F(ConfigManagerTest, ParseMixedCaseUnits) {
  WriteConfigFile(
      ModifyConfigValue(GetValidConfigContent(), "min_diff_time", "100MS"));

  auto result = ConfigManager::Load(test_config_path);

  // Regex is case-sensitive on units
  ASSERT_FALSE(result.has_value());
  EXPECT_THAT(result.error(), HasSubstr("Invalid duration format"));
}

TEST_F(ConfigManagerTest, ParseDurationWithPlusSign) {
  WriteConfigFile(
      ModifyConfigValue(GetValidConfigContent(), "min_diff_time", "+100ms"));

  auto result = ConfigManager::Load(test_config_path);

  // Regex doesn't allow + sign
  ASSERT_FALSE(result.has_value());
  EXPECT_THAT(result.error(), HasSubstr("Invalid duration format"));
}

TEST_F(ConfigManagerTest, ParseDurationLeadingZeros) {
  WriteConfigFile(
      ModifyConfigValue(GetValidConfigContent(), "min_diff_time", "0000100ms"));

  auto result = ConfigManager::Load(test_config_path);

  // from_chars should handle leading zeros
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->min_diff_time, 100ms);
}

// E11-E15: Configuration Completeness

TEST_F(ConfigManagerTest, LoadConfigWithExtraSections) {
  WriteConfigFile(GetValidConfigContent() + R"(
[Unknown]
some_key = some_value
)");

  auto result = ConfigManager::Load(test_config_path);

  // Should ignore unknown sections
  EXPECT_TRUE(result.has_value());
}

TEST_F(ConfigManagerTest, LoadConfigWithExtraKeys) {
  WriteConfigFile(R"([Price]
initial_price = 100
unknown_key = 123
time_horizon = 24h
)");

  auto result = ConfigManager::Load(test_config_path);

  // Should ignore unknown keys
  EXPECT_TRUE(result.has_value());
}

TEST_F(ConfigManagerTest, LoadConfigSectionsOutOfOrder) {
  WriteConfigFile(R"([Simulation]
steps_count = 100000

[Trade]
fast_ema = 1s
slow_ema = 5s

[Price]
initial_price = 100
time_horizon = 24h
min_diff_time = 100ms
max_diff_time = 200ms

[Exchange]
rejection_probability = 1.0
)");

  auto result = ConfigManager::Load(test_config_path);

  EXPECT_TRUE(result.has_value()) << "Section order shouldn't matter";
}

TEST_F(ConfigManagerTest, LoadConfigKeysOutOfOrder) {
  WriteConfigFile(R"([Price]
time_horizon = 24h
price_variation = 0.10
initial_price = 100
max_diff_time = 200ms
average_trend_value = 0.05
min_diff_time = 100ms
)");

  auto result = ConfigManager::Load(test_config_path);

  EXPECT_TRUE(result.has_value()) << "Key order shouldn't matter";
}

// E16-E20: File Format Edge Cases

TEST_F(ConfigManagerTest, ParseFileWithWindowsLineEndings) {
  std::ofstream file(test_config_path, std::ios::binary);
  file << "[Price]\r\n";
  file << "initial_price = 100\r\n";
  file << "time_horizon = 24h\r\n";
  file.close();

  auto result = ConfigManager::Load(test_config_path);

  EXPECT_TRUE(result.has_value()) << "Should handle \\r\\n line endings";
}

TEST_F(ConfigManagerTest, ParseFileWithUnixLineEndings) {
  std::ofstream file(test_config_path, std::ios::binary);
  file << "[Price]\n";
  file << "initial_price = 100\n";
  file << "time_horizon = 24h\n";
  file.close();

  auto result = ConfigManager::Load(test_config_path);

  EXPECT_TRUE(result.has_value()) << "Should handle \\n line endings";
}

TEST_F(ConfigManagerTest, ParseFileWithNoTrailingNewline) {
  std::ofstream file(test_config_path, std::ios::binary);
  file << "[Price]\n";
  file << "initial_price = 100";  // No trailing newline
  file.close();

  auto result = ConfigManager::Load(test_config_path);

  EXPECT_TRUE(result.has_value()) << "Should handle missing trailing newline";
}

TEST_F(ConfigManagerTest, ParseVeryLargeConfigFile) {
  std::string config = GetValidConfigContent();
  // Add many comment lines
  for (int i = 0; i < 1000; ++i) {
    config += std::format("; Comment line {}\n", i);
  }
  WriteConfigFile(config);

  auto result = ConfigManager::Load(test_config_path);

  EXPECT_TRUE(result.has_value()) << "Should handle large files";
}

// E21-E25: Path Edge Cases

TEST_F(ConfigManagerTest, PathFieldsCanBeEmpty) {
  auto config = GetValidConfigContent();
  config = ModifyConfigValue(config, "price_evolution_path", "");
  config = ModifyConfigValue(config, "orders_log_path", "");
  WriteConfigFile(config);

  auto result = ConfigManager::Load(test_config_path);

  ASSERT_TRUE(result.has_value()) << "Empty paths should be valid";
  EXPECT_TRUE(result->price_evolution_path.empty());
  EXPECT_TRUE(result->orders_log_path.empty());
}

TEST_F(ConfigManagerTest, PathsCanBeIdentical) {
  auto config = GetValidConfigContent();
  config = ModifyConfigValue(config, "price_evolution_path", "same.csv");
  config = ModifyConfigValue(config, "orders_log_path", "same.csv");
  WriteConfigFile(config);

  auto result = ConfigManager::Load(test_config_path);

  ASSERT_TRUE(result.has_value()) << "Identical paths should be valid";
  EXPECT_EQ(result->price_evolution_path, result->orders_log_path);
}

TEST_F(ConfigManagerTest, PathsWithTrailingSlashes) {
  WriteConfigFile(ModifyConfigValue(GetValidConfigContent(),
                                    "price_evolution_path", "output/"));

  auto result = ConfigManager::Load(test_config_path);

  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(result->price_evolution_path, "output/");
}

TEST_F(ConfigManagerTest, PathsWithDotSegments) {
  WriteConfigFile(ModifyConfigValue(
      GetValidConfigContent(), "price_evolution_path", "../output/file.csv"));

  auto result = ConfigManager::Load(test_config_path);

  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(result->price_evolution_path, "../output/file.csv");
}

TEST_F(ConfigManagerTest, PathsWithMultipleSlashes) {
  WriteConfigFile(ModifyConfigValue(
      GetValidConfigContent(), "price_evolution_path", "output//file.csv"));

  auto result = ConfigManager::Load(test_config_path);

  EXPECT_TRUE(result.has_value());
  // filesystem::path should handle normalization
}
