// src/controller/traffic_controller.hpp
//
// NetBucket — Traffic Controller
//
// What is this? 
//
// TrafficController is the brain of the system. It is the single component
// that all packets pass through.
//
// Its job:
//   1. Receive a packet
//   2. Refill the token bucket (lazy)
//   3. Try to consume tokens for the packet
//   4. If allowed → call the forward callback
//   5. If denied and SHAPING → enqueue the packet
//   6. If denied and POLICING → count as dropped
//   7. Update statistics
//
// Why a forward callback? 
//
// The TrafficController knows HOW to make a shaping/policing decision,
// but it should NOT know WHERE to forward the packet (TUN write? loopback?
// test vector?).
//
// The forwarding destination is injected at construction time as a
// std::function<void(Packet)>. This follows the Dependency Inversion
// principle and makes the controller fully unit-testable without a TUN device.
//
//  Live configuration 
//
// The Scheduler calls apply_config() whenever the ConfigManager detects a change.
// apply_config() updates the token bucket and queue without dropping state.

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

// Forward callback type: called for every packet that is allowed to pass.
using ForwardFn = std::function<void(Packet)>;

class TrafficController {
public:
    /// @param cfg      Initial configuration
    /// @param forward  Called when a packet is cleared to be forwarded
    TrafficController(const Config& cfg, ForwardFn forward);

    ~TrafficController();

    // Non-copyable/non-movable — owns threads
    TrafficController(const TrafficController&)            = delete;
    TrafficController& operator=(const TrafficController&) = delete;

    // Core operation 

    /// Submit a packet for processing (called from packet ingestion thread).
    ///
    /// Thread-safe. Returns immediately — packet is either forwarded or queued.
    void process(Packet packet);

    //  Configuration 

    /// Apply new configuration live (called from CLI/Config thread).
    void apply_config(const Config& cfg);

    // Lifecycle 

    void start();   ///< Start the scheduler background thread
    void stop();    ///< Signal stop and join all threads

    // Inspection 

    [[nodiscard]] const StatisticsEngine& stats()     const { return stats_; }
    [[nodiscard]] TrafficMode             mode()       const;
    [[nodiscard]] double                  token_level() const;
    [[nodiscard]] double                  capacity()    const;
    [[nodiscard]] double                  rate_bps()    const;

private:
    // Scheduler thread 

    /// Background thread that drains the packet queue when tokens are available.
    ///
    /// Loop:
    ///   1. Wait for a packet in the queue (condition_variable, non-busy)
    ///   2. Peek at front packet — try to consume tokens
    ///   3. If enough tokens → pop and forward
    ///   4. If not enough   → sleep until tokens for front packet would be ready
    void scheduler_loop();

    // Data members 

    ForwardFn           forward_fn_;
    TokenBucket         bucket_;
    PacketQueue         queue_;
    StatisticsEngine    stats_;

    std::atomic<TrafficMode>  mode_;
    std::atomic<bool>         running_{false};

    std::thread scheduler_thread_;
};

} // namespace netbucket
