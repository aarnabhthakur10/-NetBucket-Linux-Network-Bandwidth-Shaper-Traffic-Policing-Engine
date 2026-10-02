// src/controller/traffic_controller.cpp
#include "controller/traffic_controller.hpp"
#include "logger/logger.hpp"

#include <chrono>
#include <thread>

namespace netbucket {

// ─── Construction ─────────────────────────────────────────────────────────────

TrafficController::TrafficController(const Config& cfg, ForwardFn forward)
    : forward_fn_{std::move(forward)}
    , bucket_{cfg.rate_bps, cfg.bucket_capacity_bytes}
    , queue_{cfg.queue_capacity_packets}
    , mode_{cfg.mode}
{
    NB_LOG_INFO("TrafficController", "Created: mode=" +
        ConfigManager::mode_to_string(cfg.mode) +
        " rate=" + std::to_string(cfg.rate_bps / 1e6) + " Mbps");
}

TrafficController::~TrafficController() {
    stop();
}

// ─── Lifecycle ────────────────────────────────────────────────────────────────

void TrafficController::start() {
    if (running_.exchange(true)) return;   // Already started
    NB_LOG_INFO("TrafficController", "Starting scheduler thread");
    scheduler_thread_ = std::thread{[this] { scheduler_loop(); }};
}

void TrafficController::stop() {
    if (!running_.exchange(false)) return;   // Already stopped
    NB_LOG_INFO("TrafficController", "Stopping");
    queue_.shutdown();
    if (scheduler_thread_.joinable()) scheduler_thread_.join();
    NB_LOG_INFO("TrafficController", "Stopped");
}

// ─── Core operation ───────────────────────────────────────────────────────────

void TrafficController::process(Packet packet) {
    stats_.record_received(packet.size_bytes);

    const double sz = static_cast<double>(packet.size_bytes);

    if (bucket_.consume(sz)) {
        // Token bucket has enough — forward immediately
        stats_.record_forwarded(packet.size_bytes);
        forward_fn_(std::move(packet));
        return;
    }

    // Insufficient tokens — decision depends on mode
    const TrafficMode m = mode_.load(std::memory_order_relaxed);

    if (m == TrafficMode::SHAPING) {
        // Queue the packet; the scheduler will forward it later
        if (!queue_.push(std::move(packet))) {
            // Queue full — overflow drop
            stats_.record_dropped(packet.size_bytes);
            stats_.record_queue_overflow();
            NB_LOG_DEBUG("TrafficController", "Queue overflow — packet dropped");
        } else {
            stats_.record_queued(packet.size_bytes);
            stats_.update_queue_depth(queue_.size());
        }
    } else {
        // POLICING — drop immediately
        stats_.record_dropped(packet.size_bytes);
    }
}

// ─── Scheduler thread ─────────────────────────────────────────────────────────
//
// This thread continuously tries to drain the queue.
//
// Design: We do NOT busy-wait.
//
// The condition_variable in PacketQueue::wait_for_packet() blocks this thread
// until a new packet arrives. Once woken, we compute how long until the
// front packet's tokens will be available, then sleep for exactly that duration.
//
// Why compute sleep duration?
//   At 10 Mbps, a 1500-byte packet needs:
//   1500 * 8 / 10,000,000 = 1.2 ms
//   We should sleep ~1.2 ms, then re-check. Busy-polling every nanosecond
//   would waste an entire CPU core.
//
// Sleep calculation:
//   tokens_needed = front_packet.size_bytes - current_tokens
//   seconds_needed = tokens_needed / rate_bytes_sec
//   sleep_ns = seconds_needed * 1e9

void TrafficController::scheduler_loop() {
    NB_LOG_DEBUG("TrafficController", "Scheduler thread started");

    while (running_.load(std::memory_order_relaxed)) {
        // Block until queue has a packet (or 100ms timeout for shutdown check)
        if (!queue_.wait_for_packet(std::chrono::milliseconds{100})) {
            continue;   // Timeout — check running_ and loop
        }

        // Try to forward as many queued packets as tokens allow
        while (true) {
            auto opt = queue_.pop();
            if (!opt) break;   // Queue empty

            Packet pkt = std::move(*opt);
            const double sz = static_cast<double>(pkt.size_bytes);

            if (bucket_.consume(sz)) {
                // Tokens available — forward this packet
                const uint64_t wait_ns = static_cast<uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::steady_clock::now() - pkt.enqueued_at
                    ).count()
                );
                stats_.record_wait_ns(wait_ns);
                stats_.record_forwarded(pkt.size_bytes);
                stats_.record_dequeued(pkt.size_bytes);
                stats_.update_queue_depth(queue_.size());
                forward_fn_(std::move(pkt));
            } else {
                // Not enough tokens yet — put the packet back and sleep
                // We have to push it back first because pop() already removed it
                queue_.push(std::move(pkt));

                // Calculate sleep duration: how long until enough tokens accumulate?
                const double tokens_now     = bucket_.tokens();
                const double tokens_deficit = sz - tokens_now;
                const double rate_bps_val   = bucket_.rate_bps();
                const double rate_bytes_sec = rate_bps_val / 8.0;
                const double seconds_needed = (rate_bytes_sec > 0.0)
                    ? tokens_deficit / rate_bytes_sec
                    : 0.001;   // Fallback 1ms

                const auto sleep_us = static_cast<int64_t>(seconds_needed * 1e6);
                const auto clamp_us = std::max(int64_t{100}, std::min(sleep_us, int64_t{100'000}));

                std::this_thread::sleep_for(std::chrono::microseconds{clamp_us});
                break;   // Re-enter outer loop to try again
            }
        }
    }

    NB_LOG_DEBUG("TrafficController", "Scheduler thread exiting");
}

// ─── Configuration ────────────────────────────────────────────────────────────

void TrafficController::apply_config(const Config& cfg) {
    bucket_.set_rate(cfg.rate_bps);
    bucket_.set_capacity(cfg.bucket_capacity_bytes);
    queue_.set_capacity(cfg.queue_capacity_packets);
    mode_.store(cfg.mode, std::memory_order_relaxed);

    NB_LOG_INFO("TrafficController",
        "Config updated: rate=" + std::to_string(cfg.rate_bps / 1e6) +
        " Mbps, mode=" + ConfigManager::mode_to_string(cfg.mode));
}

// ─── Inspection ───────────────────────────────────────────────────────────────

TrafficMode TrafficController::mode() const {
    return mode_.load(std::memory_order_relaxed);
}

double TrafficController::token_level() const {
    return bucket_.tokens();
}

double TrafficController::capacity() const {
    return bucket_.capacity_bytes();
}

double TrafficController::rate_bps() const {
    return bucket_.rate_bps();
}

} // namespace netbucket
