#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "config/Config.h"
#include "logs/TickLogger.h"

using namespace std::chrono_literals;
using ::testing::HasSubstr;

namespace fs = std::filesystem;

// ============================================================================
// Test Fixture
// ============================================================================

class TickLoggerTest : public ::testing::Test {
 protected:
  fs::path temp_dir;
  fs::path test_file_path;

  void SetUp() override {
    auto timestamp =
        std::chrono::system_clock::now().time_since_epoch().count();
    temp_dir = fs::temp_directory_path() /
               std::format("tick_logger_test_{}", timestamp);
    fs::create_directories(temp_dir);
    test_file_path = temp_dir / "ticks.csv";
  }

  void TearDown() override {
    std::error_code ec;
    fs::remove_all(temp_dir, ec);
  }

  Config CreateTestConfig() {
    Config cfg;
    cfg.price_evolution_path = test_file_path;
    return cfg;
  }

  std::string ReadFileContent() {
    std::ifstream file(test_file_path);
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
  }

  std::vector<std::string> ReadFileLines() {
    std::vector<std::string> lines;
    std::ifstream file(test_file_path);
    std::string line;
    while (std::getline(file, line)) {
      lines.push_back(line);
    }
    return lines;
  }
};

// ============================================================================
// Constructor Tests
// ============================================================================

TEST_F(TickLoggerTest, Constructor_ValidPath_CreatesFile) {
  Config cfg = CreateTestConfig();

  {
    TickLogger logger(cfg);
  }  // Destroy logger to release file

  EXPECT_TRUE(fs::exists(test_file_path));
}

TEST_F(TickLoggerTest, Constructor_CreatesDirectories) {
  Config cfg;
  fs::path nested_path = temp_dir / "subdir1" / "subdir2" / "ticks.csv";
  cfg.price_evolution_path = nested_path;

  {
    TickLogger logger(cfg);
  }  // Destroy logger to release file

  EXPECT_TRUE(fs::exists(nested_path));
}

TEST_F(TickLoggerTest, Constructor_InvalidPath_Throws) {
  Config cfg;
#ifdef _WIN32
  cfg.price_evolution_path = "Z:\\nonexistent\\path\\file.csv";
#else
  cfg.price_evolution_path = "/nonexistent/path/that/does/not/exist/file.csv";
#endif

  EXPECT_THROW(TickLogger logger(cfg), std::runtime_error);
}

// ============================================================================
// writeTick Tests
// ============================================================================

TEST_F(TickLoggerTest, WriteTick_ValidTick_WritesToFile) {
  Config cfg = CreateTestConfig();
  std::optional<std::string> result;

  {
    TickLogger logger(cfg);
    Tick tick{1000ms, 100.5, 50.25};
    result = logger.writeTick(tick);
  }  // Destroy logger to release file

  EXPECT_FALSE(result.has_value());
  std::string content = ReadFileContent();
  EXPECT_THAT(content, HasSubstr("100.500"));
  EXPECT_THAT(content, HasSubstr("50.250"));
}

TEST_F(TickLoggerTest, WriteTick_ReturnsNulloptOnSuccess) {
  Config cfg = CreateTestConfig();
  std::optional<std::string> result;

  {
    TickLogger logger(cfg);
    Tick tick{1000ms, 100.0, 50.0};
    result = logger.writeTick(tick);
  }

  EXPECT_EQ(result, std::nullopt);
}

TEST_F(TickLoggerTest, WriteTick_MultipleTicks_AllWritten) {
  Config cfg = CreateTestConfig();

  {
    TickLogger logger(cfg);
    logger.writeTick({100ms, 100.0, 50.0});
    logger.writeTick({200ms, 101.0, 51.0});
    logger.writeTick({300ms, 102.0, 52.0});
  }  // Destroy logger to release file

  auto lines = ReadFileLines();
  EXPECT_EQ(lines.size(), 4);  // Header + 3 ticks
}

TEST_F(TickLoggerTest, WriteTick_PriceFormat_3Decimals) {
  Config cfg = CreateTestConfig();

  {
    TickLogger logger(cfg);
    Tick tick{1000ms, 123.456789, 50.0};
    logger.writeTick(tick);
  }

  std::string content = ReadFileContent();
  EXPECT_THAT(content, HasSubstr("123.457"));  // Rounded to 3 decimals
}

TEST_F(TickLoggerTest, WriteTick_VolumeFormat_3Decimals) {
  Config cfg = CreateTestConfig();

  {
    TickLogger logger(cfg);
    Tick tick{1000ms, 100.0, 78.9012345};
    logger.writeTick(tick);
  }

  std::string content = ReadFileContent();
  EXPECT_THAT(content, HasSubstr("78.901"));  // Rounded to 3 decimals
}

TEST_F(TickLoggerTest, WriteTick_TimestampFormat_Correct) {
  Config cfg = CreateTestConfig();

  {
    TickLogger logger(cfg);
    auto timestamp = 1h + 30min + 45s + 500ms;
    Tick tick{timestamp, 100.0, 50.0};
    logger.writeTick(tick);
  }

  std::string content = ReadFileContent();
  EXPECT_THAT(content, HasSubstr("01:30:45"));
}

TEST_F(TickLoggerTest, WriteTick_ZeroTimestamp) {
  Config cfg = CreateTestConfig();
  std::optional<std::string> result;

  {
    TickLogger logger(cfg);
    Tick tick{0ns, 100.0, 50.0};
    result = logger.writeTick(tick);
  }

  EXPECT_EQ(result, std::nullopt);
  std::string content = ReadFileContent();
  EXPECT_THAT(content, HasSubstr("00:00:00"));
}

TEST_F(TickLoggerTest, WriteTick_LargeVolume_HandlesCorrectly) {
  Config cfg = CreateTestConfig();
  std::optional<std::string> result;

  {
    TickLogger logger(cfg);
    Tick tick{1000ms, 100.0, 1e10};
    result = logger.writeTick(tick);
  }

  EXPECT_EQ(result, std::nullopt);
}

TEST_F(TickLoggerTest, WriteTick_ZeroPrice) {
  Config cfg = CreateTestConfig();
  std::optional<std::string> result;

  {
    TickLogger logger(cfg);
    Tick tick{1000ms, 0.0, 50.0};
    result = logger.writeTick(tick);
  }

  EXPECT_EQ(result, std::nullopt);
  std::string content = ReadFileContent();
  EXPECT_THAT(content, HasSubstr("0.000"));
}

TEST_F(TickLoggerTest, WriteTick_CSVFormat_CommaSeparated) {
  Config cfg = CreateTestConfig();

  {
    TickLogger logger(cfg);
    Tick tick{1s, 100.0, 50.0};
    logger.writeTick(tick);
  }

  auto lines = ReadFileLines();
  ASSERT_GE(lines.size(), 2);
  EXPECT_NE(lines[1].find(','), std::string::npos);
}

TEST_F(TickLoggerTest, WriteTick_SequentialWrites_MaintainOrder) {
  Config cfg = CreateTestConfig();

  {
    TickLogger logger(cfg);
    for (int i = 1; i <= 5; ++i) {
      logger.writeTick({std::chrono::seconds(i), 100.0 + i, 50.0 + i});
    }
  }

  auto lines = ReadFileLines();
  EXPECT_EQ(lines.size(), 6);  // Header + 5 ticks
}
