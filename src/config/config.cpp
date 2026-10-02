// src/config/config.cpp
#include "config/config.hpp"

#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace netbucket {

// ─── Validation ───────────────────────────────────────────────────────────────

void ConfigManager::validate(const Config& c) {
    if (c.rate_bps <= 0.0)
        throw std::invalid_argument("rate_bps must be > 0");
    if (c.rate_bps > 10e9)
        throw std::invalid_argument("rate_bps > 10 Gbps is unreasonably large");
    if (c.bucket_capacity_bytes < 1500.0)
        throw std::invalid_argument("bucket_capacity_bytes must be >= 1500 (one max Ethernet frame)");
    if (c.queue_capacity_packets == 0)
        throw std::invalid_argument("queue_capacity_packets must be >= 1");
    if (c.queue_capacity_packets > 100'000)
        throw std::invalid_argument("queue_capacity_packets > 100,000 is unreasonably large");
}

// ─── Load ─────────────────────────────────────────────────────────────────────

void ConfigManager::load(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open())
        throw std::runtime_error("Cannot open config file: " + path);

    nlohmann::json j;
    try {
        file >> j;
    } catch (const nlohmann::json::exception& e) {
        throw std::runtime_error(
            std::string("JSON parse error in ") + path + ": " + e.what()
        );
    }

    Config c;

    if (j.contains("rate_bps"))
        c.rate_bps = j["rate_bps"].get<double>();

    if (j.contains("bucket_capacity_bytes"))
        c.bucket_capacity_bytes = j["bucket_capacity_bytes"].get<double>();

    if (j.contains("queue_capacity_packets"))
        c.queue_capacity_packets = j["queue_capacity_packets"].get<std::size_t>();

    if (j.contains("mode"))
        c.mode = parse_mode(j["mode"].get<std::string>());

    if (j.contains("interface"))
        c.interface = j["interface"].get<std::string>();

    if (j.contains("log_level"))
        c.log_level = j["log_level"].get<std::string>();

    if (j.contains("stats_interval_ms"))
        c.stats_interval_ms = j["stats_interval_ms"].get<uint32_t>();

    validate(c);

    std::unique_lock lock{mutex_};
    config_ = c;
}

// ─── Save ─────────────────────────────────────────────────────────────────────

void ConfigManager::save(const std::string& path) const {
    std::shared_lock lock{mutex_};

    nlohmann::json j;
    j["rate_bps"]               = config_.rate_bps;
    j["bucket_capacity_bytes"]  = config_.bucket_capacity_bytes;
    j["queue_capacity_packets"] = config_.queue_capacity_packets;
    j["mode"]                   = mode_to_string(config_.mode);
    j["interface"]              = config_.interface;
    j["log_level"]              = config_.log_level;
    j["stats_interval_ms"]      = config_.stats_interval_ms;

    lock.unlock();

    std::ofstream f(path);
    if (!f.is_open())
        throw std::runtime_error("Cannot write config file: " + path);

    f << j.dump(4) << "\n";
}

// ─── Accessors ────────────────────────────────────────────────────────────────

Config ConfigManager::get() const {
    std::shared_lock lock{mutex_};
    return config_;
}

// ─── Live update setters ──────────────────────────────────────────────────────

void ConfigManager::set_rate(double rate_bps) {
    if (rate_bps <= 0.0)
        throw std::invalid_argument("rate_bps must be > 0");
    std::unique_lock lock{mutex_};
    config_.rate_bps = rate_bps;
}

void ConfigManager::set_capacity(double bytes) {
    if (bytes <= 0.0)
        throw std::invalid_argument("bucket_capacity_bytes must be > 0");
    std::unique_lock lock{mutex_};
    config_.bucket_capacity_bytes = bytes;
}

void ConfigManager::set_queue_capacity(std::size_t packets) {
    if (packets == 0)
        throw std::invalid_argument("queue_capacity_packets must be >= 1");
    std::unique_lock lock{mutex_};
    config_.queue_capacity_packets = packets;
}

void ConfigManager::set_mode(TrafficMode mode) {
    std::unique_lock lock{mutex_};
    config_.mode = mode;
}

// ─── Mode string conversion ───────────────────────────────────────────────────

TrafficMode ConfigManager::parse_mode(const std::string& s) {
    if (s == "shaping")  return TrafficMode::SHAPING;
    if (s == "policing") return TrafficMode::POLICING;
    throw std::invalid_argument(
        "Unknown mode '" + s + "'. Use 'shaping' or 'policing'."
    );
}

std::string ConfigManager::mode_to_string(TrafficMode m) {
    switch (m) {
        case TrafficMode::SHAPING:  return "shaping";
        case TrafficMode::POLICING: return "policing";
    }
    return "unknown";
}

} // namespace netbucket
