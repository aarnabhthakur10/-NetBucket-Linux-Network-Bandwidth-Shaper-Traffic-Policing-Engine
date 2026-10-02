// tests/unit/test_config.cpp
//
// NetBucket — ConfigManager Unit Tests

#include "config/config.hpp"

#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>

using namespace netbucket;

// Helper: write a JSON config file to a temp path
static std::string write_temp_config(const std::string& content) {
    const std::string path = "/tmp/netbucket_test_config.json";
    std::ofstream f(path);
    f << content;
    return path;
}

// ─── Test 1: Default config values ───────────────────────────────────────────

TEST(ConfigManager, DefaultValues) {
    ConfigManager cm;
    const auto cfg = cm.get();
    EXPECT_GT(cfg.rate_bps, 0.0);
    EXPECT_GT(cfg.bucket_capacity_bytes, 0.0);
    EXPECT_GT(cfg.queue_capacity_packets, 0u);
}

// ─── Test 2: Load valid JSON ───────────────────────────────────────────────────

TEST(ConfigManager, LoadValidJson) {
    const auto path = write_temp_config(R"({
        "rate_bps": 5000000,
        "bucket_capacity_bytes": 1048576,
        "queue_capacity_packets": 500,
        "mode": "policing"
    })");

    ConfigManager cm;
    EXPECT_NO_THROW(cm.load(path));

    const auto cfg = cm.get();
    EXPECT_DOUBLE_EQ(cfg.rate_bps, 5000000.0);
    EXPECT_DOUBLE_EQ(cfg.bucket_capacity_bytes, 1048576.0);
    EXPECT_EQ(cfg.queue_capacity_packets, 500u);
    EXPECT_EQ(cfg.mode, TrafficMode::POLICING);
}

// ─── Test 3: Reject zero rate ─────────────────────────────────────────────────

TEST(ConfigManager, RejectZeroRate) {
    const auto path = write_temp_config(R"({"rate_bps": 0})");
    ConfigManager cm;
    EXPECT_THROW(cm.load(path), std::exception);
}

// ─── Test 4: Reject very small capacity ──────────────────────────────────────

TEST(ConfigManager, RejectTooSmallCapacity) {
    const auto path = write_temp_config(
        R"({"rate_bps": 1000000, "bucket_capacity_bytes": 100})");
    ConfigManager cm;
    EXPECT_THROW(cm.load(path), std::exception);
}

// ─── Test 5: set_rate validates ───────────────────────────────────────────────

TEST(ConfigManager, SetRateValidation) {
    ConfigManager cm;
    EXPECT_THROW(cm.set_rate(0.0),  std::invalid_argument);
    EXPECT_THROW(cm.set_rate(-1.0), std::invalid_argument);
    EXPECT_NO_THROW(cm.set_rate(1'000'000.0));
    EXPECT_DOUBLE_EQ(cm.get().rate_bps, 1'000'000.0);
}

// ─── Test 6: parse_mode / mode_to_string round-trip ──────────────────────────

TEST(ConfigManager, ModeRoundTrip) {
    EXPECT_EQ(ConfigManager::parse_mode("shaping"),  TrafficMode::SHAPING);
    EXPECT_EQ(ConfigManager::parse_mode("policing"), TrafficMode::POLICING);
    EXPECT_EQ(ConfigManager::mode_to_string(TrafficMode::SHAPING),  "shaping");
    EXPECT_EQ(ConfigManager::mode_to_string(TrafficMode::POLICING), "policing");
    EXPECT_THROW(ConfigManager::parse_mode("invalid"), std::invalid_argument);
}

// ─── Test 7: CLI rate parser ──────────────────────────────────────────────────

#include "cli/cli.hpp"

TEST(CLI, ParseRate) {
    EXPECT_DOUBLE_EQ(CLI::parse_rate("10Mbps"),  10'000'000.0);
    EXPECT_DOUBLE_EQ(CLI::parse_rate("1Gbps"),   1'000'000'000.0);
    EXPECT_DOUBLE_EQ(CLI::parse_rate("500Kbps"), 500'000.0);
    EXPECT_NEAR(CLI::parse_rate("10mbit"), 10'000'000.0, 1.0);
    EXPECT_THROW(CLI::parse_rate("0Mbps"), std::exception);
    EXPECT_THROW(CLI::parse_rate("notanumber"), std::exception);
}

// ─── Test 8: CLI bytes parser ─────────────────────────────────────────────────

TEST(CLI, ParseBytes) {
    EXPECT_DOUBLE_EQ(CLI::parse_bytes("20MB"), 20.0 * 1048576.0);
    EXPECT_DOUBLE_EQ(CLI::parse_bytes("1MB"),  1048576.0);
    EXPECT_DOUBLE_EQ(CLI::parse_bytes("512KB"), 512.0 * 1024.0);
    EXPECT_THROW(CLI::parse_bytes("0MB"), std::exception);
}
