// tests/unit/test_packet_queue.cpp
//
// NetBucket — PacketQueue Unit Tests

#include "core/packet.hpp"
#include "core/packet_queue.hpp"

#include <gtest/gtest.h>
#include <thread>
#include <vector>

using namespace netbucket;

static Packet make_pkt(std::size_t sz = 1500) {
    return Packet::make_synthetic(sz);
}

// ─── Test 1: FIFO order preserved ────────────────────────────────────────────

TEST(PacketQueue, FIFOOrder) {
    PacketQueue q{10};

    auto p1 = make_pkt(100);
    auto p2 = make_pkt(200);
    auto p3 = make_pkt(300);

    const uint64_t id1 = p1.id, id2 = p2.id, id3 = p3.id;

    q.push(std::move(p1));
    q.push(std::move(p2));
    q.push(std::move(p3));

    EXPECT_EQ(q.pop()->id, id1);
    EXPECT_EQ(q.pop()->id, id2);
    EXPECT_EQ(q.pop()->id, id3);
}

// ─── Test 2: Queue capacity enforced ─────────────────────────────────────────

TEST(PacketQueue, QueueCapacityEnforced) {
    PacketQueue q{3};

    EXPECT_TRUE(q.push(make_pkt()));
    EXPECT_TRUE(q.push(make_pkt()));
    EXPECT_TRUE(q.push(make_pkt()));
    EXPECT_EQ(q.size(), 3u);

    // Fourth packet must be rejected
    EXPECT_FALSE(q.push(make_pkt()));
    EXPECT_EQ(q.size(), 3u);   // Queue size unchanged
}

// ─── Test 3: Overflow increments drop counter ────────────────────────────────

TEST(PacketQueue, OverflowIncrementsDrop) {
    PacketQueue q{2};
    q.push(make_pkt());
    q.push(make_pkt());

    EXPECT_EQ(q.stats().overflow_drops, 0u);

    q.push(make_pkt());   // This should fail
    EXPECT_EQ(q.stats().overflow_drops, 1u);

    q.push(make_pkt());   // Another fail
    EXPECT_EQ(q.stats().overflow_drops, 2u);
}

// ─── Test 4: Pop from empty returns nullopt ───────────────────────────────────

TEST(PacketQueue, PopEmptyReturnsNullopt) {
    PacketQueue q{10};
    auto result = q.pop();
    EXPECT_FALSE(result.has_value());
}

// ─── Test 5: Enqueue/dequeue counters correct ─────────────────────────────────

TEST(PacketQueue, StatisticsCounters) {
    PacketQueue q{100};

    for (int i = 0; i < 5; ++i) q.push(make_pkt());
    EXPECT_EQ(q.stats().enqueued, 5u);
    EXPECT_EQ(q.stats().current_depth, 5u);

    for (int i = 0; i < 3; ++i) q.pop();
    EXPECT_EQ(q.stats().dequeued, 3u);
    EXPECT_EQ(q.stats().current_depth, 2u);
}

// ─── Test 6: Max depth tracked ────────────────────────────────────────────────

TEST(PacketQueue, MaxDepthTracked) {
    PacketQueue q{100};

    for (int i = 0; i < 10; ++i) q.push(make_pkt());
    EXPECT_EQ(q.stats().max_depth, 10u);

    // Drain some
    for (int i = 0; i < 5; ++i) q.pop();
    EXPECT_EQ(q.stats().max_depth, 10u);   // Max is historical — should not decrease

    // Add fewer than the previous peak
    for (int i = 0; i < 3; ++i) q.push(make_pkt());
    EXPECT_EQ(q.stats().max_depth, 10u);   // Still 10
}

// ─── Test 7: Queue empty/size ─────────────────────────────────────────────────

TEST(PacketQueue, EmptyAndSize) {
    PacketQueue q{10};
    EXPECT_TRUE(q.empty());
    EXPECT_EQ(q.size(), 0u);

    q.push(make_pkt());
    EXPECT_FALSE(q.empty());
    EXPECT_EQ(q.size(), 1u);

    q.pop();
    EXPECT_TRUE(q.empty());
    EXPECT_EQ(q.size(), 0u);
}

// ─── Test 8: Shutdown unblocks wait_for_packet ────────────────────────────────

TEST(PacketQueue, ShutdownUnblocksWaiter) {
    PacketQueue q{10};

    // Start a thread that blocks waiting for a packet
    bool unblocked = false;
    std::thread waiter{[&]() {
        q.wait_for_packet(std::chrono::milliseconds{5000});
        unblocked = true;
    }};

    // Give the thread time to start
    std::this_thread::sleep_for(std::chrono::milliseconds{50});
    EXPECT_FALSE(unblocked);

    q.shutdown();
    waiter.join();
    EXPECT_TRUE(unblocked);
}

// ─── Test 9: Concurrent push/pop is safe ──────────────────────────────────────

TEST(PacketQueue, ConcurrentPushPopIsSafe) {
    PacketQueue q{10000};

    const int N = 1000;
    std::atomic<int> produced{0}, consumed{0};

    std::thread producer{[&]() {
        for (int i = 0; i < N; ++i) {
            if (q.push(make_pkt())) ++produced;
        }
    }};

    std::thread consumer{[&]() {
        int got = 0;
        while (got < N) {
            auto p = q.pop();
            if (p) { ++consumed; ++got; }
        }
    }};

    producer.join();
    consumer.join();

    // No crash = thread safety OK
    // produced should equal N, consumed should equal N
    EXPECT_EQ(produced.load(), N);
    EXPECT_EQ(consumed.load(), N);
}
