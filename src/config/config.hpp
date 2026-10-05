#pragma once

#include <cstdint>
#include <shared_mutex>
#include <string>

namespace netbucket {

enum class TrafficMode {
    SHAPING,
    POLICING,
};

struct Config {
    double       rate_bps               = 10'000'000.0;
    double       bucket_capacity_bytes  = 20'971'520.0;
    std::size_t  queue_capacity_packets = 1'000;
    TrafficMode  mode                   = TrafficMode::SHAPING;
    std::string  interface              = "tun0";
    std::string  log_level              = "info";
    uint32_t     stats_interval_ms      = 1'000;
};

class ConfigManager {
public:
    ConfigManager() = default;

    void load(const std::string& path);

    void save(const std::string& path) const;

    [[nodiscard]] Config get() const;

    void set_rate(double rate_bps);

    void set_capacity(double bytes);

    void set_queue_capacity(std::size_t packets);

    void set_mode(TrafficMode mode);

    static TrafficMode parse_mode(const std::string& s);
    static std::string mode_to_string(TrafficMode m);

private:
    mutable std::shared_mutex mutex_;
    Config config_;

    static void validate(const Config& c);
};

}
