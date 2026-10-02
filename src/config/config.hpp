// src/config/config.hpp
//
// NetBucket — Configuration Manager
//
// ─── Responsibilities ────────────────────────────────────────────────────────
//
// 1. Parse and validate JSON configuration files
// 2. Provide a thread-safe configuration store
// 3. Expose typed accessors for all parameters
// 4. Apply live updates (rate, capacity, mode, queue size)
//
// ─── Why std::shared_mutex? ──────────────────────────────────────────────────
//
// Configuration is read on every packet (to check mode: shaping/policing).
// Configuration is written rarely (operator CLI command or REST call).
//
// std::shared_mutex allows MULTIPLE simultaneous readers (packet threads)
// and EXCLUSIVE access for a single writer (config change).
//
// Using std::mutex would serialize all packet-thread reads behind each other —
// unnecessary because reads don't mutate state.
//
// Using atomics for each field wouldn't protect a multi-field atomic update
// (changing rate AND capacity simultaneously must appear atomic to readers).
//
// std::shared_mutex + std::shared_lock (readers) + std::unique_lock (writer)
// is the correct solution.

#pragma once

#include <cstdint>
#include <shared_mutex>
#include <string>

namespace netbucket {

enum class TrafficMode {
    SHAPING,    ///< Excess packets are queued; long-term rate is preserved
    POLICING,   ///< Excess packets are dropped immediately
};

struct Config {
    double       rate_bps               = 10'000'000.0;    ///< bits/sec
    double       bucket_capacity_bytes  = 20'971'520.0;    ///< bytes (20 MB)
    std::size_t  queue_capacity_packets = 1'000;
    TrafficMode  mode                   = TrafficMode::SHAPING;
    std::string  interface              = "tun0";
    std::string  log_level              = "info";
    uint32_t     stats_interval_ms      = 1'000;
};

class ConfigManager {
public:
    ConfigManager() = default;

    // ── Load / save ───────────────────────────────────────────────────────────

    /// Load and validate from a JSON file.
    /// @throws std::runtime_error on file I/O or validation failure.
    void load(const std::string& path);

    /// Save current configuration to JSON file.
    void save(const std::string& path) const;

    // ── Thread-safe accessors ─────────────────────────────────────────────────

    [[nodiscard]] Config get() const;

    // ── Live update setters (validated) ───────────────────────────────────────

    /// @throws std::invalid_argument if rate_bps <= 0
    void set_rate(double rate_bps);

    /// @throws std::invalid_argument if bytes <= 0
    void set_capacity(double bytes);

    /// @throws std::invalid_argument if packets == 0
    void set_queue_capacity(std::size_t packets);

    void set_mode(TrafficMode mode);

    /// Parse "shaping" or "policing" string into TrafficMode.
    static TrafficMode parse_mode(const std::string& s);
    static std::string mode_to_string(TrafficMode m);

private:
    mutable std::shared_mutex mutex_;
    Config config_;

    static void validate(const Config& c);
};

} // namespace netbucket
