// src/core/packet_queue.cpp
#include "core/packet_queue.hpp"

#include <algorithm>   // std::max

namespace netbucket {

// Construction 

PacketQueue::PacketQueue(std::size_t capacity)
    : capacity_{capacity == 0 ? 1 : capacity}
{}

// push 

bool PacketQueue::push(Packet packet) {
    std::lock_guard lock{mutex_};

    if (shutdown_) return false;

    if (queue_.size() >= capacity_) {
        // Queue is full — tail-drop this packet
        ++stats_.overflow_drops;
        return false;
    }

    // Record the moment the packet entered the queue
    packet.enqueued_at = Clock::now();

    stats_.bytes_queued += packet.size_bytes;
    ++stats_.enqueued;
    ++stats_.current_depth;
    stats_.max_depth = std::max(stats_.max_depth, stats_.current_depth);

    queue_.push(std::move(packet));

    // Notify the scheduler thread (if it is blocked in wait_for_packet)
    cv_.notify_one();

    return true;
}

// pop 

std::optional<Packet> PacketQueue::pop() {
    std::lock_guard lock{mutex_};

    if (queue_.empty()) return std::nullopt;

    Packet p = std::move(queue_.front());
    queue_.pop();

    // Update wait-time statistics
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

// wait_for_packet 

bool PacketQueue::wait_for_packet(std::chrono::milliseconds timeout) {
    std::unique_lock lock{mutex_};
    cv_.wait_for(lock, timeout, [this] {
        return !queue_.empty() || shutdown_;
    });
    return !queue_.empty();
}

// Inspection 

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
    return stats_;   // Copy — safe to return by value
}

// Configuration 

void PacketQueue::set_capacity(std::size_t capacity) {
    std::lock_guard lock{mutex_};
    capacity_ = (capacity == 0) ? 1 : capacity;
}

// Shutdown 

void PacketQueue::shutdown() {
    {
        std::lock_guard lock{mutex_};
        shutdown_ = true;
    }
    cv_.notify_all();   // Wake all waiting threads so they can check shutdown_
}

} // namespace netbucket
