// src/cli/cli.hpp
//
// NetBucket — Command-Line Interface
//
// The CLI provides the operator interface for controlling the traffic shaper.
//
// Design:
//   CLI parses arguments → validates → calls ConfigManager setters
//   → TrafficController::apply_config() picks up the change
//
// This ensures the CLI and REST API (Phase 15) share identical validation
// because both go through ConfigManager.
//
// Commands:
//   netbucket start [--config FILE]
//   netbucket stop
//   netbucket status
//   netbucket stats
//   netbucket reset
//   netbucket set-rate   <value>   (e.g. 10Mbps, 1Gbps, 500Kbps)
//   netbucket set-burst  <value>   (e.g. 20MB, 1MB)
//   netbucket set-mode   <shaping|policing>
//   netbucket set-queue  <packets>

#pragma once

#include "config/config.hpp"

#include <functional>
#include <string>
#include <vector>

namespace netbucket {

class TrafficController;
class StatisticsEngine;

class CLI {
public:
    /// @param config_mgr  Configuration manager to update on commands
    /// @param controller  Traffic controller to query for live status (may be nullptr before start)
    CLI(ConfigManager& config_mgr,
        TrafficController* controller = nullptr);

    void set_controller(TrafficController* c) { controller_ = c; }

    /// Parse and execute a command from argc/argv.
    /// @returns exit code (0 = success, 1 = error)
    int run(int argc, char* argv[]);

    /// Parse and execute a command from a vector of strings (for testing).
    int run(const std::vector<std::string>& args);

    //  Utility: parse human-readable values 

    /// Parse "10Mbps", "1Gbps", "500Kbps", "1000000" → bits/second
    static double parse_rate(const std::string& s);

    /// Parse "20MB", "1MB", "512KB", "65536" → bytes
    static double parse_bytes(const std::string& s);

private:
    int cmd_start  (const std::vector<std::string>& args);
    int cmd_stop   ();
    int cmd_status ();
    int cmd_stats  ();
    int cmd_reset  ();
    int cmd_set_rate  (const std::string& value);
    int cmd_set_burst (const std::string& value);
    int cmd_set_mode  (const std::string& mode);
    int cmd_set_queue (const std::string& value);
    int cmd_help   ();

    void print_status() const;

    ConfigManager&    config_mgr_;
    TrafficController* controller_;
};

} // namespace netbucket
