// src/core/packet_queue.hpp
//
// NetBucket — FIFO Packet Queue
//
// ─── What is this? ────────────────────────────────────────────────────────────
//
// PacketQueue is the holding area for packets that cannot currently be
// forwarded because the token bucket is empty.
//
// This only applies in SHAPING mode. In POLICING mode, a packet is dropped
// immediately when tokens are insufficient — the queue is never used.
//
// ─── FIFO — why? ──────────────────────────────────────────────────────────────
//
// First-In, First-Out is the simplest correct approach. It:
//   - Preserves packet order (important for TCP)
//   - Is trivial to implement and verify
//   - Has O(1) enqueue and dequeue
//
// Later phases can add priority queuing (packets from different classes get
// different queues), but FIFO is the correct foundation.
//
// ─── Capacity ─────────────────────────────────────────────────────────────────
//
// The queue has a hard maximum depth (in packets).
// When the queue is full, new packets are TAIL-DROPPED (the newest packet
// is dropped, not the oldest). This is the simplest drop policy.
//
// Why a cap? Without it, an infinite burst of traffic would exhaust all
// system memory. The cap bounds memory usage and forces the shaper to
// keep up with the inflow or admit that it is falling behind.
//
// ─── Thread Safety ────────────────────────────────────────────────────────────
//
// All methods are protected by a std::mutex.
//
// The Scheduler thread calls pop() in a loop.
// The Packet ingestion thread calls push() when a packet arrives.
// Both threads must be able to run concurrently without corrupting the queue.
//
// std::condition_variable notifies the scheduler when the queue transitions
// from empty to non-empty. This avoids busy-waiting.

#pragma once

#include "core/packet.hpp"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <queue>

namespace netbucket {

// ─── QueueStats ───────────────────────────────────────────────────────────────

struct QueueStats {
    uint64_t enqueued         = 0;   ///< Total packets ever enqueued
    uint64_t dequeued         = 0;   ///< Total packets ever dequeued
    uint64_t overflow_drops   = 0;   ///< Packets dropped because queue was full
    uint64_t bytes_queued     = 0;   ///< Total bytes ever enqueued
    std::size_t current_depth = 0;   ///< Current number of packets in queue
    std::size_t max_depth     = 0;   ///< Peak depth ever observed

    // Latency tracking (nanoseconds)
    uint64_t total_wait_ns    = 0;   ///< Sum of all wait times (for average)
    uint64_t max_wait_ns      = 0;   ///< Maximum single packet wait time
};

// ─── PacketQueue ──────────────────────────────────────────────────────────────

class PacketQueue {
public:
    /// @param capacity  Maximum number of packets the queue can hold (>= 1)
    explicit PacketQueue(std::size_t capacity);

    // Non-copyable (owns mutex and condition variable)
    PacketQueue(const PacketQueue&)            = delete;
    PacketQueue& operator=(const PacketQueue&) = delete;

    // ── Operations ────────────────────────────────────────────────────────────

    /// Enqueue a packet (thread-safe).
    ///
    /// Sets packet.enqueued_at to the current time.
    /// If queue is full, increments overflow_drops and returns false.
    ///
    /// @returns true  if packet was enqueued
    /// @returns false if queue is full (packet discarded)
    bool push(Packet packet);

    /// Dequeue the front packet (thread-safe, non-blocking).
    ///
    /// Updates wait-time statistics.
    ///
    /// @returns The front packet, or std::nullopt if the queue is empty.
    [[nodiscard]] std::optional<Packet> pop();

    /// Block the calling thread until the queue is non-empty or shutdown.
    /// Used by the Scheduler thread to avoid busy-waiting.
    ///
    /// @param timeout  Maximum time to wait
    /// @returns true if queue has packets, false if timeout or shutdown
    bool wait_for_packet(std::chrono::milliseconds timeout =
                         std::chrono::milliseconds{100});

    // ── Inspection ────────────────────────────────────────────────────────────

    [[nodiscard]] bool        empty()    const;
    [[nodiscard]] std::size_t size()     const;
    [[nodiscard]] std::size_t capacity() const { return capacity_; }

    /// Snapshot of queue statistics (copy for thread safety)
    [[nodiscard]] QueueStats stats() const;

    // ── Configuration ─────────────────────────────────────────────────────────

    void set_capacity(std::size_t capacity);

    // ── Shutdown ──────────────────────────────────────────────────────────────

    /// Signal the scheduler thread to stop waiting.
    /// Called during graceful shutdown.
    void shutdown();

private:
    mutable std::mutex      mutex_;
    std::condition_variable cv_;        ///< Notified when queue goes from empty to non-empty

    std::queue<Packet>      queue_;
    std::size_t             capacity_;
    bool                    shutdown_  = false;

    QueueStats              stats_;
};

} // namespace netbucket
