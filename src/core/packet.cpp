// src/core/packet.cpp
#include "core/packet.hpp"

#include <arpa/inet.h>    // ntohs, ntohl (Linux POSIX)
#include <cstring>        // memcpy

namespace netbucket {

// Static counter initialised to 0; incremented atomically per factory call.
uint64_t Packet::next_id_ = 0;

// Factory: synthetic 

Packet Packet::make_synthetic(std::size_t size_bytes) {
    Packet p;
    p.id         = ++next_id_;
    p.size_bytes = size_bytes;
    p.arrived_at = Clock::now();
    // data remains empty — signals "synthetic" to callers
    return p;
}

// Factory: from real TUN bytes 
//
// An IP packet arriving from the TUN device starts with the IP header.
// IPv4 header structure (RFC 791):
//   Byte 0:      Version (4 bits) + IHL (4 bits)
//   Byte 8:      TTL
//   Byte 9:      Protocol (6=TCP, 17=UDP, 1=ICMP)
//   Bytes 12–15: Source IP
//   Bytes 16–19: Destination IP
//   Bytes 20+:   TCP/UDP header (if IHL == 5)
//
// TCP header (RFC 793):
//   Bytes 0–1:   Source port
//   Bytes 2–3:   Destination port
//
// UDP header (RFC 768):
//   Bytes 0–1:   Source port
//   Bytes 2–3:   Destination port

Packet Packet::from_bytes(std::vector<uint8_t> raw) {
    Packet p;
    p.id         = ++next_id_;
    p.size_bytes = raw.size();
    p.arrived_at = Clock::now();
    p.data       = std::move(raw);

    const auto& d = p.data;

    // Need at least a minimal IPv4 header (20 bytes)
    if (d.size() < 20) return p;

    const uint8_t version_ihl = d[0];
    const uint8_t version = (version_ihl >> 4) & 0x0F;

    if (version != 4) return p;   // Only IPv4 for now

    const uint8_t ihl_words = version_ihl & 0x0F;    // header length in 32-bit words
    const std::size_t ip_header_len = static_cast<std::size_t>(ihl_words) * 4;

    p.protocol = d[9];

    // Extract source/dest IP (network byte order → host byte order)
    uint32_t src_raw = 0, dst_raw = 0;
    std::memcpy(&src_raw, &d[12], 4);
    std::memcpy(&dst_raw, &d[16], 4);
    p.src_ip = ntohl(src_raw);
    p.dst_ip = ntohl(dst_raw);

    // Extract ports from TCP or UDP header
    if ((p.protocol == 6 || p.protocol == 17) &&
        d.size() >= ip_header_len + 4)
    {
        uint16_t sp = 0, dp = 0;
        std::memcpy(&sp, &d[ip_header_len + 0], 2);
        std::memcpy(&dp, &d[ip_header_len + 2], 2);
        p.src_port = ntohs(sp);
        p.dst_port = ntohs(dp);
    }

    return p;
}

// Helpers 

std::chrono::nanoseconds Packet::queue_wait_time() const {
    // If enqueued_at was never set (zero TimePoint), return zero duration.
    if (enqueued_at == TimePoint{}) return std::chrono::nanoseconds{0};
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        Clock::now() - enqueued_at
    );
}

} // namespace netbucket
