#include "statistics/statistics.hpp"

#include <algorithm>
#include <cstring>
#include <iomanip>
#include <sstream>

namespace netbucket {

void StatisticsEngine::record_received(std::size_t bytes) {
    pkt_received_.fetch_add(1, std::memory_order_relaxed);
    bytes_received_.fetch_add(bytes, std::memory_order_relaxed);
}

void StatisticsEngine::record_forwarded(std::size_t bytes) {
    pkt_forwarded_.fetch_add(1, std::memory_order_relaxed);
    bytes_forwarded_.fetch_add(bytes, std::memory_order_relaxed);
}

void StatisticsEngine::record_dropped(std::size_t bytes) {
    pkt_dropped_.fetch_add(1, std::memory_order_relaxed);
    bytes_dropped_.fetch_add(bytes, std::memory_order_relaxed);
}

void StatisticsEngine::record_queued(std::size_t bytes) {
    pkt_queued_.fetch_add(1, std::memory_order_relaxed);
    (void)bytes;
}

void StatisticsEngine::record_dequeued(std::size_t bytes) {
    pkt_dequeued_.fetch_add(1, std::memory_order_relaxed);
    (void)bytes;
}

void StatisticsEngine::update_queue_depth(std::size_t depth) {
    queue_depth_.store(depth, std::memory_order_relaxed);
    std::size_t current_max = max_queue_depth_.load(std::memory_order_relaxed);
    while (depth > current_max) {
        if (max_queue_depth_.compare_exchange_weak(current_max, depth,
                std::memory_order_relaxed)) break;
    }
}

void StatisticsEngine::record_queue_overflow() {
    queue_overflows_.fetch_add(1, std::memory_order_relaxed);
}

void StatisticsEngine::record_wait_ns(uint64_t ns) {
    total_wait_ns_.fetch_add(ns, std::memory_order_relaxed);
    uint64_t cur_max = max_wait_ns_.load(std::memory_order_relaxed);
    while (ns > cur_max) {
        if (max_wait_ns_.compare_exchange_weak(cur_max, ns,
                std::memory_order_relaxed)) break;
    }
}

void StatisticsEngine::update_token_level(
    [[maybe_unused]] double tokens_bytes,
    [[maybe_unused]] double capacity_bytes)
{
}

StatisticsEngine::ThroughputSnapshot StatisticsEngine::snapshot_throughput() {
    using Clock = std::chrono::steady_clock;
    const auto now = Clock::now();
    const std::chrono::duration<double> elapsed = now - snap_time_;

    const uint64_t recv = bytes_received_.load(std::memory_order_relaxed);
    const uint64_t fwd  = bytes_forwarded_.load(std::memory_order_relaxed);
    const uint64_t drp  = bytes_dropped_.load(std::memory_order_relaxed);

    ThroughputSnapshot snap;
    if (elapsed.count() > 0.0) {
        snap.incoming_bps = static_cast<double>(recv - snap_bytes_received_) * 8.0 / elapsed.count();
        snap.outgoing_bps = static_cast<double>(fwd  - snap_bytes_forwarded_) * 8.0 / elapsed.count();
        snap.dropped_bps  = static_cast<double>(drp  - snap_bytes_dropped_)   * 8.0 / elapsed.count();
    }

    snap_bytes_received_  = recv;
    snap_bytes_forwarded_ = fwd;
    snap_bytes_dropped_   = drp;
    snap_time_            = now;

    return snap;
}

void StatisticsEngine::reset() {
    pkt_received_  .store(0); pkt_forwarded_ .store(0);
    pkt_dropped_   .store(0); pkt_queued_    .store(0);
    pkt_dequeued_  .store(0);
    bytes_received_.store(0); bytes_forwarded_.store(0);
    bytes_dropped_ .store(0);
    queue_depth_   .store(0); max_queue_depth_.store(0);
    queue_overflows_.store(0);
    total_wait_ns_ .store(0); max_wait_ns_    .store(0);

    snap_bytes_received_  = 0;
    snap_bytes_forwarded_ = 0;
    snap_bytes_dropped_   = 0;
    snap_time_            = std::chrono::steady_clock::now();
}

std::string StatisticsEngine::format_status(
    double rate_bps,
    double capacity_bytes,
    double tokens_bytes) const
{
    const auto recv = pkt_received_.load();
    const auto fwd  = pkt_forwarded_.load();
    const auto drp  = pkt_dropped_.load();
    const auto qd   = queue_depth_.load();
    const auto maxw_ms = static_cast<double>(max_wait_ns_.load()) / 1'000'000.0;

    auto pct = [](uint64_t a, uint64_t b) -> double {
        return b > 0 ? 100.0 * static_cast<double>(a) / static_cast<double>(b) : 0.0;
    };

    std::ostringstream o;
    o << std::fixed << std::setprecision(2);
    o << "\n";
    o << "  Rate:       " << (rate_bps / 1'000'000.0)   << " Mbps\n";
    o << "  Capacity:   " << (capacity_bytes / 1'048'576.0) << " MB\n";
    o << "  Tokens:     " << (tokens_bytes / 1'048'576.0)   << " MB\n";
    o << "  Queue:      " << qd << " packets\n";
    o << "\n";
    o << "  Received:   " << recv << "\n";
    o << "  Forwarded:  " << fwd  << " (" << pct(fwd, recv) << "%)\n";
    o << "  Dropped:    " << drp  << " (" << pct(drp, recv) << "%)\n";
    o << "\n";
    o << "  Max Q wait: " << maxw_ms << " ms\n";
    return o.str();
}

}
