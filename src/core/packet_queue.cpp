#include "core/packet_queue.hpp"

#include <algorithm>

namespace netbucket {

PacketQueue::PacketQueue(std::size_t capacity)
    : capacity_{capacity == 0 ? 1 : capacity}
{}

bool PacketQueue::push(Packet packet) {
    std::lock_guard lock{mutex_};

    if (shutdown_) return false;

    if (queue_.size() >= capacity_) {
        ++stats_.overflow_drops;
        return false;
    }

    packet.enqueued_at = Clock::now();

    stats_.bytes_queued += packet.size_bytes;
    ++stats_.enqueued;
    ++stats_.current_depth;
    stats_.max_depth = std::max(stats_.max_depth, stats_.current_depth);

    queue_.push(std::move(packet));

    cv_.notify_one();

    return true;
}

std::optional<Packet> PacketQueue::pop() {
    std::lock_guard lock{mutex_};

    if (queue_.empty()) return std::nullopt;

    Packet p = std::move(queue_.front());
    queue_.pop();

    const auto wait_ns = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            Clock::now() - p.enqueued_at
        ).count()
    );
    stats_.total_wait_ns += wait_ns;
    stats_.max_wait_ns    = std::max(stats_.max_wait_ns, wait_ns);

    ++stats_.dequeued;
    --stats_.current_depth;

    return p;
}

bool PacketQueue::wait_for_packet(std::chrono::milliseconds timeout) {
    std::unique_lock lock{mutex_};
    cv_.wait_for(lock, timeout, [this] {
        return !queue_.empty() || shutdown_;
    });
    return !queue_.empty();
}

bool PacketQueue::empty() const {
    std::lock_guard lock{mutex_};
    return queue_.empty();
}

std::size_t PacketQueue::size() const {
    std::lock_guard lock{mutex_};
    return queue_.size();
}

QueueStats PacketQueue::stats() const {
    std::lock_guard lock{mutex_};
    return stats_;
}

void PacketQueue::set_capacity(std::size_t capacity) {
    std::lock_guard lock{mutex_};
    capacity_ = (capacity == 0) ? 1 : capacity;
}

void PacketQueue::shutdown() {
    {
        std::lock_guard lock{mutex_};
        shutdown_ = true;
    }
    cv_.notify_all();
}

}
