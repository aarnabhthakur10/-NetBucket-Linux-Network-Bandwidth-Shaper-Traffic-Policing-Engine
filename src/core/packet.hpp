// src/core/packet.hpp
//
// NetBucket — Packet Abstraction
//
// What is this?
//
// A Packet is the fundamental unit of work in NetBucket.
//
// In Phase 2–4, Packet is a pure C++ struct with no Linux dependencies.
// The `data` field can hold raw bytes read from a TUN device.
//
// In Phase 5 (TUN/TAP), PacketReader fills real Packet objects from the
// Linux TUN file descriptor. Everything else (TrafficController, Queue,
// Statistics) operates on Packet objects — they never care whether the
// bytes came from real traffic or a synthetic test.
//
// Design decision: struct not class 
//
// Packet is a plain data aggregate. It has no invariants to enforce, no
// private state, and no behavior. Using a struct with public members is
// appropriate here and avoids unnecessary accessor boilerplate.
//
//  Fields 
//
// Keep fields minimal. Do not add a field unless something actually uses it.
//
// id               — unique monotonic ID, useful for tracing in logs/tests
// size_bytes       — number of bytes in the packet payload (determines token cost)
// data             — raw packet bytes (empty during simulation phases)
// arrived_at       — steady_clock timestamp when packet entered the system
// enqueued_at      — set when packet is placed in the PacketQueue
// flow_id          — assigned by TrafficClassifier (Phase 10); default = 0
// src_ip, dst_ip   — extracted from IP header (Phase 5+); zero during simulation
// src_port         — from TCP/UDP header (Phase 5+)
// dst_port         — from TCP/UDP header (Phase 5+)
// protocol         — IP protocol number (6=TCP, 17=UDP, 1=ICMP)

#pragma once

#include <chrono>
#include <cstdint>
#include <vector>

namespace netbucket {

using Clock     = std::chrono::steady_clock;
using TimePoint = Clock::time_point;

// Flow ID 0 means "unclassified" — handled by the default bucket.
using FlowId = uint32_t;
constexpr FlowId DEFAULT_FLOW_ID = 0;

struct Packet {
    // Identity 
    uint64_t    id          = 0;              ///< Unique packet ID (monotonic)
    std::size_t size_bytes  = 0;              ///< Payload size in bytes (token cost)

    // Payload 
    // Raw bytes from TUN device. Empty vector = synthetic/simulation packet.
    // When real TUN integration is active, data.size() == size_bytes.
    std::vector<uint8_t> data;

    // Timestamps (steady_clock) 
    TimePoint arrived_at;    ///< When the packet entered the traffic engine
    TimePoint enqueued_at;   ///< When the packet was placed in the queue (if queued)

    //  Classification 
    FlowId    flow_id  = DEFAULT_FLOW_ID;  ///< Assigned by TrafficClassifier
    uint32_t  src_ip   = 0;               ///< Source IPv4 address (network byte order)
    uint32_t  dst_ip   = 0;               ///< Destination IPv4 address
    uint16_t  src_port = 0;               ///< Source port (TCP/UDP)
    uint16_t  dst_port = 0;               ///< Destination port (TCP/UDP)
    uint8_t   protocol = 0;               ///< IP protocol (6=TCP, 17=UDP, 1=ICMP)

    // Factory 

    /// Create a synthetic packet for unit testing.
    /// Automatically assigns a monotonically increasing ID.
    /// No payload bytes are allocated.
    static Packet make_synthetic(std::size_t size_bytes);

    /// Create a real packet from raw bytes (used by PacketReader).
    /// Parses IP header to extract src/dst addresses and ports.
    static Packet from_bytes(std::vector<uint8_t> data);

    // Helpers 

    /// How long did this packet wait in the queue?
    /// Returns zero duration if packet was never enqueued.
    [[nodiscard]] std::chrono::nanoseconds queue_wait_time() const;

    [[nodiscard]] bool is_synthetic() const { return data.empty(); }

private:
    // Monotonic counter shared across all packets.
    // Initialized once, incremented on each make_synthetic / from_bytes call.
    static uint64_t next_id_;
};

} // namespace netbucket
