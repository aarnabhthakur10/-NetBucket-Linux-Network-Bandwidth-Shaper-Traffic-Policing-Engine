#include "controller/traffic_controller.hpp"
#include "logger/logger.hpp"

#include <chrono>
#include <thread>

namespace netbucket {

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

void TrafficController::start() {
    if (running_.exchange(true)) return;
    NB_LOG_INFO("TrafficController", "Starting scheduler thread");
    scheduler_thread_ = std::thread{[this] { scheduler_loop(); }};
}

void TrafficController::stop() {
    if (!running_.exchange(false)) return;
    NB_LOG_INFO("TrafficController", "Stopping");
    queue_.shutdown();
    if (scheduler_thread_.joinable()) scheduler_thread_.join();
    NB_LOG_INFO("TrafficController", "Stopped");
}

void TrafficController::process(Packet packet) {
    stats_.record_received(packet.size_bytes);

    const double sz = static_cast<double>(packet.size_bytes);

    if (bucket_.consume(sz)) {
        stats_.record_forwarded(packet.size_bytes);
        forward_fn_(std::move(packet));
        return;
    }

    const TrafficMode m = mode_.load(std::memory_order_relaxed);

    if (m == TrafficMode::SHAPING) {
        if (!queue_.push(std::move(packet))) {
            stats_.record_dropped(packet.size_bytes);
            stats_.record_queue_overflow();
            NB_LOG_DEBUG("TrafficController", "Queue overflow — packet dropped");
        } else {
            stats_.record_queued(packet.size_bytes);
            stats_.update_queue_depth(queue_.size());
        }
    } else {
        stats_.record_dropped(packet.size_bytes);
    }
}

void TrafficController::scheduler_loop() {
    NB_LOG_DEBUG("TrafficController", "Scheduler thread started");

    while (running_.load(std::memory_order_relaxed)) {
        if (!queue_.wait_for_packet(std::chrono::milliseconds{100})) {
            continue;
        }

        while (true) {
            auto opt = queue_.pop();
            if (!opt) break;

            Packet pkt = std::move(*opt);
            const double sz = static_cast<double>(pkt.size_bytes);

            if (bucket_.consume(sz)) {
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
                queue_.push(std::move(pkt));

                const double tokens_now     = bucket_.tokens();
                const double tokens_deficit = sz - tokens_now;
                const double rate_bps_val   = bucket_.rate_bps();
                const double rate_bytes_sec = rate_bps_val / 8.0;
                const double seconds_needed = (rate_bytes_sec > 0.0)
                    ? tokens_deficit / rate_bytes_sec
                    : 0.001;

                const auto sleep_us = static_cast<int64_t>(seconds_needed * 1e6);
                const auto clamp_us = std::max(int64_t{100}, std::min(sleep_us, int64_t{100'000}));

                std::this_thread::sleep_for(std::chrono::microseconds{clamp_us});
                break;
            }
        }
    }

    NB_LOG_DEBUG("TrafficController", "Scheduler thread exiting");
}

void TrafficController::apply_config(const Config& cfg) {
    bucket_.set_rate(cfg.rate_bps);
    bucket_.set_capacity(cfg.bucket_capacity_bytes);
    queue_.set_capacity(cfg.queue_capacity_packets);
    mode_.store(cfg.mode, std::memory_order_relaxed);

    NB_LOG_INFO("TrafficController",
        "Config updated: rate=" + std::to_string(cfg.rate_bps / 1e6) +
        " Mbps, mode=" + ConfigManager::mode_to_string(cfg.mode));
}

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

}
