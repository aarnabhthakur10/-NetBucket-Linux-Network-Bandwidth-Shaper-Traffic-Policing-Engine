#pragma once

#include "core/packet.hpp"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <queue>

namespace netbucket {

struct QueueStats {
    uint64_t enqueued         = 0;
    uint64_t dequeued         = 0;
    uint64_t overflow_drops   = 0;
    uint64_t bytes_queued     = 0;
    std::size_t current_depth = 0;
    std::size_t max_depth     = 0;

    uint64_t total_wait_ns    = 0;
    uint64_t max_wait_ns      = 0;
};

class PacketQueue {
public:
    explicit PacketQueue(std::size_t capacity);

    PacketQueue(const PacketQueue&)            = delete;
    PacketQueue& operator=(const PacketQueue&) = delete;

    bool push(Packet packet);

    [[nodiscard]] std::optional<Packet> pop();

    bool wait_for_packet(std::chrono::milliseconds timeout =
                         std::chrono::milliseconds{100});

    [[nodiscard]] bool        empty()    const;
    [[nodiscard]] std::size_t size()     const;
    [[nodiscard]] std::size_t capacity() const { return capacity_; }

    [[nodiscard]] QueueStats stats() const;

    void set_capacity(std::size_t capacity);

    void shutdown();

private:
    mutable std::mutex      mutex_;
    std::condition_variable cv_;

    std::queue<Packet>      queue_;
    std::size_t             capacity_;
    bool                    shutdown_  = false;

    QueueStats              stats_;
};

}
