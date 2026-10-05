#pragma once

#include <chrono>
#include <cstdint>
#include <vector>

namespace netbucket {

using Clock     = std::chrono::steady_clock;
using TimePoint = Clock::time_point;

using FlowId = uint32_t;
constexpr FlowId DEFAULT_FLOW_ID = 0;

struct Packet {
    uint64_t    id          = 0;
    std::size_t size_bytes  = 0;

    std::vector<uint8_t> data;

    TimePoint arrived_at;
    TimePoint enqueued_at;

    FlowId    flow_id  = DEFAULT_FLOW_ID;
    uint32_t  src_ip   = 0;
    uint32_t  dst_ip   = 0;
    uint16_t  src_port = 0;
    uint16_t  dst_port = 0;
    uint8_t   protocol = 0;

    static Packet make_synthetic(std::size_t size_bytes);

    static Packet from_bytes(std::vector<uint8_t> data);

    [[nodiscard]] std::chrono::nanoseconds queue_wait_time() const;

    [[nodiscard]] bool is_synthetic() const { return data.empty(); }

private:
    static uint64_t next_id_;
};

}
