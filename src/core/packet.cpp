#include "core/packet.hpp"

#include <arpa/inet.h>
#include <cstring>

namespace netbucket {

uint64_t Packet::next_id_ = 0;

Packet Packet::make_synthetic(std::size_t size_bytes) {
    Packet p;
    p.id         = ++next_id_;
    p.size_bytes = size_bytes;
    p.arrived_at = Clock::now();
    return p;
}

Packet Packet::from_bytes(std::vector<uint8_t> raw) {
    Packet p;
    p.id         = ++next_id_;
    p.size_bytes = raw.size();
    p.arrived_at = Clock::now();
    p.data       = std::move(raw);

    const auto& d = p.data;

    if (d.size() < 20) return p;

    const uint8_t version_ihl = d[0];
    const uint8_t version = (version_ihl >> 4) & 0x0F;

    if (version != 4) return p;

    const uint8_t ihl_words = version_ihl & 0x0F;
    const std::size_t ip_header_len = static_cast<std::size_t>(ihl_words) * 4;

    p.protocol = d[9];

    uint32_t src_raw = 0, dst_raw = 0;
    std::memcpy(&src_raw, &d[12], 4);
    std::memcpy(&dst_raw, &d[16], 4);
    p.src_ip = ntohl(src_raw);
    p.dst_ip = ntohl(dst_raw);

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

std::chrono::nanoseconds Packet::queue_wait_time() const {
    if (enqueued_at == TimePoint{}) return std::chrono::nanoseconds{0};
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        Clock::now() - enqueued_at
    );
}

}
