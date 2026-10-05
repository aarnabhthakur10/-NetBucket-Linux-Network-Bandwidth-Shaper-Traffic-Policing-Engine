#pragma once

#include "config/config.hpp"
#include "core/packet.hpp"
#include "core/packet_queue.hpp"
#include "core/token_bucket.hpp"
#include "statistics/statistics.hpp"

#include <atomic>
#include <functional>
#include <memory>
#include <thread>

namespace netbucket {

using ForwardFn = std::function<void(Packet)>;

class TrafficController {
public:
    TrafficController(const Config& cfg, ForwardFn forward);

    ~TrafficController();

    TrafficController(const TrafficController&)            = delete;
    TrafficController& operator=(const TrafficController&) = delete;

    void process(Packet packet);

    void apply_config(const Config& cfg);

    void start();
    void stop();

    [[nodiscard]] const StatisticsEngine& stats()     const { return stats_; }
    [[nodiscard]] TrafficMode             mode()       const;
    [[nodiscard]] double                  token_level() const;
    [[nodiscard]] double                  capacity()    const;
    [[nodiscard]] double                  rate_bps()    const;

private:

    void scheduler_loop();

    ForwardFn           forward_fn_;
    TokenBucket         bucket_;
    PacketQueue         queue_;
    StatisticsEngine    stats_;

    std::atomic<TrafficMode>  mode_;
    std::atomic<bool>         running_{false};

    std::thread scheduler_thread_;
};

}
