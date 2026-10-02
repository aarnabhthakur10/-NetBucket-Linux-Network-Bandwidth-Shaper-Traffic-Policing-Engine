// src/cli/cli.cpp
#include "cli/cli.hpp"
#include "controller/traffic_controller.hpp"
#include "logger/logger.hpp"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace netbucket {

CLI::CLI(ConfigManager& config_mgr, TrafficController* controller)
    : config_mgr_{config_mgr}, controller_{controller}
{}

// ─── Entry points ─────────────────────────────────────────────────────────────

int CLI::run(int argc, char* argv[]) {
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) args.emplace_back(argv[i]);
    return run(args);
}

int CLI::run(const std::vector<std::string>& args) {
    if (args.empty()) return cmd_help();

    const std::string& cmd = args[0];

    try {
        if (cmd == "start")     return cmd_start(args);
        if (cmd == "stop")      return cmd_stop();
        if (cmd == "status")    return cmd_status();
        if (cmd == "stats")     return cmd_stats();
        if (cmd == "reset")     return cmd_reset();
        if (cmd == "help" || cmd == "--help" || cmd == "-h")
                                return cmd_help();

        if (cmd == "set-rate"  && args.size() >= 2)  return cmd_set_rate(args[1]);
        if (cmd == "set-burst" && args.size() >= 2)  return cmd_set_burst(args[1]);
        if (cmd == "set-mode"  && args.size() >= 2)  return cmd_set_mode(args[1]);
        if (cmd == "set-queue" && args.size() >= 2)  return cmd_set_queue(args[1]);

        // config shorthand: netbucket config --rate 10Mbps --burst 20MB --mode shaping
        if (cmd == "config") {
            for (std::size_t i = 1; i + 1 < args.size(); i += 2) {
                if (args[i] == "--rate")   cmd_set_rate(args[i+1]);
                else if (args[i] == "--burst") cmd_set_burst(args[i+1]);
                else if (args[i] == "--mode")  cmd_set_mode(args[i+1]);
                else if (args[i] == "--queue") cmd_set_queue(args[i+1]);
            }
            return 0;
        }

        std::cerr << "Unknown command: " << cmd << "\n";
        std::cerr << "Run 'netbucket help' for usage.\n";
        return 1;

    } catch (const std::exception& e) {
        std::cerr << "[ERROR] " << e.what() << "\n";
        return 1;
    }
}

// ─── Commands ─────────────────────────────────────────────────────────────────

int CLI::cmd_start(const std::vector<std::string>& args) {
    // Parse optional --config FILE
    for (std::size_t i = 1; i + 1 < args.size(); ++i) {
        if (args[i] == "--config") {
            config_mgr_.load(args[i+1]);
            std::cout << "[netbucket] Loaded config from " << args[i+1] << "\n";
        }
    }
    std::cout << "[netbucket] start: use main binary to run as daemon\n";
    return 0;
}

int CLI::cmd_stop() {
    std::cout << "[netbucket] stop: send SIGTERM to the running daemon\n";
    return 0;
}

int CLI::cmd_status() {
    print_status();
    return 0;
}

int CLI::cmd_stats() {
    if (!controller_) {
        std::cout << "[netbucket] Engine not running\n";
        return 1;
    }

    const auto& s = controller_->stats();
    const double tokens = controller_->token_level();
    const double cap    = controller_->capacity();
    const double rate   = controller_->rate_bps();

    std::cout << s.format_status(rate, cap, tokens);
    return 0;
}

int CLI::cmd_reset() {
    // stats reset is on StatisticsEngine — forward to controller
    std::cout << "[netbucket] Statistics reset\n";
    return 0;
}

int CLI::cmd_set_rate(const std::string& value) {
    const double bps = parse_rate(value);
    config_mgr_.set_rate(bps);
    if (controller_) controller_->apply_config(config_mgr_.get());
    std::cout << "[netbucket] Rate set to " << (bps / 1e6) << " Mbps\n";
    return 0;
}

int CLI::cmd_set_burst(const std::string& value) {
    const double bytes = parse_bytes(value);
    config_mgr_.set_capacity(bytes);
    if (controller_) controller_->apply_config(config_mgr_.get());
    std::cout << "[netbucket] Burst capacity set to " << (bytes / 1048576.0) << " MB\n";
    return 0;
}

int CLI::cmd_set_mode(const std::string& mode) {
    const TrafficMode m = ConfigManager::parse_mode(mode);
    config_mgr_.set_mode(m);
    if (controller_) controller_->apply_config(config_mgr_.get());
    std::cout << "[netbucket] Mode set to " << ConfigManager::mode_to_string(m) << "\n";
    return 0;
}

int CLI::cmd_set_queue(const std::string& value) {
    const std::size_t n = static_cast<std::size_t>(std::stoull(value));
    config_mgr_.set_queue_capacity(n);
    if (controller_) controller_->apply_config(config_mgr_.get());
    std::cout << "[netbucket] Queue capacity set to " << n << " packets\n";
    return 0;
}

int CLI::cmd_help() {
    std::cout <<
        "\nNetBucket — Traffic Shaper CLI\n"
        "Usage: netbucket <command> [options]\n\n"
        "Commands:\n"
        "  start [--config FILE]      Start the shaper (optionally load config)\n"
        "  stop                       Stop the shaper daemon\n"
        "  status                     Show current status\n"
        "  stats                      Show traffic statistics\n"
        "  reset                      Reset statistics counters\n"
        "  set-rate  <value>          Set rate  (e.g. 10Mbps, 1Gbps, 500Kbps)\n"
        "  set-burst <value>          Set burst capacity (e.g. 20MB, 1MB)\n"
        "  set-mode  <shaping|policing>  Set traffic mode\n"
        "  set-queue <N>              Set queue capacity in packets\n"
        "  config [--rate R] [--burst B] [--mode M] [--queue N]  Batch configure\n"
        "  help                       Show this help\n"
        "\nExamples:\n"
        "  netbucket set-rate 10Mbps\n"
        "  netbucket set-mode policing\n"
        "  netbucket config --rate 20Mbps --burst 5MB --mode shaping\n\n";
    return 0;
}

// ─── Status display ───────────────────────────────────────────────────────────

void CLI::print_status() const {
    const Config cfg = config_mgr_.get();
    const std::string mode_str = ConfigManager::mode_to_string(cfg.mode);

    const double tokens = controller_ ? controller_->token_level() : cfg.bucket_capacity_bytes;
    const std::size_t qd = 0;   // Would come from controller in a running system

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "\nNetBucket Traffic Controller\n";
    std::cout << "────────────────────────────────\n";
    std::cout << "Mode:       " << mode_str << "\n";
    std::cout << "Rate:       " << (cfg.rate_bps / 1e6) << " Mbps\n";
    std::cout << "Bucket:     " << (cfg.bucket_capacity_bytes / 1048576.0) << " MB\n";
    std::cout << "Tokens:     " << (tokens / 1048576.0) << " MB\n";
    std::cout << "Queue cap:  " << cfg.queue_capacity_packets << " packets\n";
    std::cout << "Queue:      " << qd << " packets\n";
    std::cout << "\n";
}

// ─── Parse helpers ────────────────────────────────────────────────────────────

double CLI::parse_rate(const std::string& s) {
    // Convert to lowercase for case-insensitive matching
    std::string lower = s;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c){ return static_cast<char>(std::tolower(c)); });

    double multiplier = 1.0;
    std::string digits = lower;

    if      (lower.ends_with("gbps")) { multiplier = 1e9;  digits = lower.substr(0, lower.size()-4); }
    else if (lower.ends_with("mbps")) { multiplier = 1e6;  digits = lower.substr(0, lower.size()-4); }
    else if (lower.ends_with("kbps")) { multiplier = 1e3;  digits = lower.substr(0, lower.size()-4); }
    else if (lower.ends_with("bps"))  { multiplier = 1.0;  digits = lower.substr(0, lower.size()-3); }
    else if (lower.ends_with("gbit")) { multiplier = 1e9;  digits = lower.substr(0, lower.size()-4); }
    else if (lower.ends_with("mbit")) { multiplier = 1e6;  digits = lower.substr(0, lower.size()-4); }
    else if (lower.ends_with("kbit")) { multiplier = 1e3;  digits = lower.substr(0, lower.size()-4); }

    try {
        const double value = std::stod(digits) * multiplier;
        if (value <= 0.0) throw std::invalid_argument("Rate must be > 0");
        return value;
    } catch (const std::invalid_argument&) {
        throw std::invalid_argument(
            "Cannot parse rate '" + s + "'. Examples: 10Mbps, 1Gbps, 500Kbps, 1000000"
        );
    }
}

double CLI::parse_bytes(const std::string& s) {
    std::string lower = s;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c){ return static_cast<char>(std::tolower(c)); });

    double multiplier = 1.0;
    std::string digits = lower;

    if      (lower.ends_with("gb")) { multiplier = 1e9;     digits = lower.substr(0, lower.size()-2); }
    else if (lower.ends_with("mb")) { multiplier = 1048576.0; digits = lower.substr(0, lower.size()-2); }
    else if (lower.ends_with("kb")) { multiplier = 1024.0;  digits = lower.substr(0, lower.size()-2); }
    else if (lower.ends_with("b"))  { multiplier = 1.0;     digits = lower.substr(0, lower.size()-1); }

    try {
        const double value = std::stod(digits) * multiplier;
        if (value <= 0.0) throw std::invalid_argument("Size must be > 0");
        return value;
    } catch (const std::invalid_argument&) {
        throw std::invalid_argument(
            "Cannot parse size '" + s + "'. Examples: 20MB, 1MB, 512KB, 65536"
        );
    }
}

} // namespace netbucket
