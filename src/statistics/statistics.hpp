#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>

namespace netbucket {

class StatisticsEngine {
public:
    StatisticsEngine() = default;

    StatisticsEngine(const StatisticsEngine&)            = delete;
    StatisticsEngine& operator=(const StatisticsEngine&) = delete;

    void record_received (std::size_t bytes);
    void record_forwarded(std::size_t bytes);
    void record_dropped  (std::size_t bytes);
    void record_queued   (std::size_t bytes);
    void record_dequeued (std::size_t bytes);

    void update_queue_depth(std::size_t depth);
    void record_queue_overflow();
    void record_wait_ns(uint64_t ns);

    void update_token_level(double tokens_bytes, double capacity_bytes);

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

    struct ThroughputSnapshot {
        double incoming_bps = 0.0;
        double outgoing_bps = 0.0;
        double dropped_bps  = 0.0;
    };
    [[nodiscard]] ThroughputSnapshot snapshot_throughput();

    void reset();

    [[nodiscard]] std::string format_status(
        double rate_bps,
        double capacity_bytes,
        double tokens_bytes) const;

private:
    std::atomic<uint64_t> pkt_received_   {0};
    std::atomic<uint64_t> pkt_forwarded_  {0};
    std::atomic<uint64_t> pkt_dropped_    {0};
    std::atomic<uint64_t> pkt_queued_     {0};
    std::atomic<uint64_t> pkt_dequeued_   {0};

    std::atomic<uint64_t> bytes_received_  {0};
    std::atomic<uint64_t> bytes_forwarded_ {0};
    std::atomic<uint64_t> bytes_dropped_   {0};

    std::atomic<std::size_t> queue_depth_     {0};
    std::atomic<std::size_t> max_queue_depth_ {0};
    std::atomic<uint64_t>    queue_overflows_ {0};
    std::atomic<uint64_t>    total_wait_ns_   {0};
    std::atomic<uint64_t>    max_wait_ns_     {0};

    uint64_t snap_bytes_received_  = 0;
    uint64_t snap_bytes_forwarded_ = 0;
    uint64_t snap_bytes_dropped_   = 0;
    std::chrono::steady_clock::time_point snap_time_ = std::chrono::steady_clock::now();
};

}
