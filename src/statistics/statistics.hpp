// src/statistics/statistics.hpp
//
// NetBucket — Statistics Engine
//
// Tracks all runtime metrics atomically so any thread can read them
// without taking a lock (reads are always eventually consistent).
//
// ─── Why std::atomic? ────────────────────────────────────────────────────────
//
// The packet thread (high frequency) increments counters on every packet.
// The stats/CLI thread reads counters periodically for reporting.
//
// A std::mutex would force the packet thread to wait for the CLI thread on
// every packet — disastrous for performance at high packet rates.
//
// std::atomic<uint64_t> with default memory_order_seq_cst is correct and
// fast. The packet thread never needs to wait.
//
// ─── Throughput calculation ───────────────────────────────────────────────────
//
// Instantaneous throughput is computed by the StatisticsEngine itself.
// It records bytes_forwarded at time T, then at time T+interval computes:
//   throughput = (bytes_forwarded_now - bytes_forwarded_prev) * 8 / interval_sec

#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>

namespace netbucket {

class StatisticsEngine {
public:
    StatisticsEngine() = default;

    // Non-copyable — owns atomic state
    StatisticsEngine(const StatisticsEngine&)            = delete;
    StatisticsEngine& operator=(const StatisticsEngine&) = delete;

    // ── Packet counters ───────────────────────────────────────────────────────
    void record_received (std::size_t bytes);
    void record_forwarded(std::size_t bytes);
    void record_dropped  (std::size_t bytes);
    void record_queued   (std::size_t bytes);
    void record_dequeued (std::size_t bytes);

    // ── Queue stats ───────────────────────────────────────────────────────────
    void update_queue_depth(std::size_t depth);
    void record_queue_overflow();
    void record_wait_ns(uint64_t ns);

    // ── Token bucket stats ────────────────────────────────────────────────────
    void update_token_level(double tokens_bytes, double capacity_bytes);

    // ── Getters ───────────────────────────────────────────────────────────────
    [[nodiscard]] uint64_t packets_received()  const { return pkt_received_.load(); }
    [[nodiscard]] uint64_t packets_forwarded() const { return pkt_forwarded_.load(); }
    [[nodiscard]] uint64_t packets_dropped()   const { return pkt_dropped_.load(); }
    [[nodiscard]] uint64_t packets_queued()    const { return pkt_queued_.load(); }
    [[nodiscard]] uint64_t bytes_received()    const { return bytes_received_.load(); }
    [[nodiscard]] uint64_t bytes_forwarded()   const { return bytes_forwarded_.load(); }
    [[nodiscard]] uint64_t bytes_dropped()     const { return bytes_dropped_.load(); }
    [[nodiscard]] uint64_t queue_overflows()   const { return queue_overflows_.load(); }
    [[nodiscard]] uint64_t total_wait_ns()     const { return total_wait_ns_.load(); }
    [[nodiscard]] uint64_t max_wait_ns()       const { return max_wait_ns_.load(); }
    [[nodiscard]] std::size_t current_queue_depth() const { return queue_depth_.load(); }
    [[nodiscard]] std::size_t max_queue_depth()     const { return max_queue_depth_.load(); }

    /// Compute throughput over the last snapshot interval.
    /// Call once per reporting period (e.g., every second).
    struct ThroughputSnapshot {
        double incoming_bps = 0.0;
        double outgoing_bps = 0.0;
        double dropped_bps  = 0.0;
    };
    [[nodiscard]] ThroughputSnapshot snapshot_throughput();

    /// Reset all counters to zero. Used by CLI `reset` command.
    void reset();

    /// Format a human-readable status block for the CLI.
    [[nodiscard]] std::string format_status(
        double rate_bps,
        double capacity_bytes,
        double tokens_bytes) const;

private:
    // Packet counters
    std::atomic<uint64_t> pkt_received_   {0};
    std::atomic<uint64_t> pkt_forwarded_  {0};
    std::atomic<uint64_t> pkt_dropped_    {0};
    std::atomic<uint64_t> pkt_queued_     {0};
    std::atomic<uint64_t> pkt_dequeued_   {0};

    // Byte counters
    std::atomic<uint64_t> bytes_received_  {0};
    std::atomic<uint64_t> bytes_forwarded_ {0};
    std::atomic<uint64_t> bytes_dropped_   {0};

    // Queue stats
    std::atomic<std::size_t> queue_depth_     {0};
    std::atomic<std::size_t> max_queue_depth_ {0};
    std::atomic<uint64_t>    queue_overflows_ {0};
    std::atomic<uint64_t>    total_wait_ns_   {0};
    std::atomic<uint64_t>    max_wait_ns_     {0};

    // Throughput tracking (previous snapshot values)
    uint64_t snap_bytes_received_  = 0;
    uint64_t snap_bytes_forwarded_ = 0;
    uint64_t snap_bytes_dropped_   = 0;
    std::chrono::steady_clock::time_point snap_time_ = std::chrono::steady_clock::now();
};

} // namespace netbucket
