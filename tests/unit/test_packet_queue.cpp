#include "core/packet.hpp"
#include "core/packet_queue.hpp"

#include <gtest/gtest.h>
#include <thread>
#include <vector>

using namespace netbucket;

static Packet make_pkt(std::size_t sz = 1500) {
    return Packet::make_synthetic(sz);
}

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

TEST(PacketQueue, QueueCapacityEnforced) {
    PacketQueue q{3};

    EXPECT_TRUE(q.push(make_pkt()));
    EXPECT_TRUE(q.push(make_pkt()));
    EXPECT_TRUE(q.push(make_pkt()));
    EXPECT_EQ(q.size(), 3u);

    EXPECT_FALSE(q.push(make_pkt()));
    EXPECT_EQ(q.size(), 3u);
}

TEST(PacketQueue, OverflowIncrementsDrop) {
    PacketQueue q{2};
    q.push(make_pkt());
    q.push(make_pkt());

    EXPECT_EQ(q.stats().overflow_drops, 0u);

    q.push(make_pkt());
    EXPECT_EQ(q.stats().overflow_drops, 1u);

    q.push(make_pkt());
    EXPECT_EQ(q.stats().overflow_drops, 2u);
}

TEST(PacketQueue, PopEmptyReturnsNullopt) {
    PacketQueue q{10};
    auto result = q.pop();
    EXPECT_FALSE(result.has_value());
}

TEST(PacketQueue, StatisticsCounters) {
    PacketQueue q{100};

    for (int i = 0; i < 5; ++i) q.push(make_pkt());
    EXPECT_EQ(q.stats().enqueued, 5u);
    EXPECT_EQ(q.stats().current_depth, 5u);

    for (int i = 0; i < 3; ++i) q.pop();
    EXPECT_EQ(q.stats().dequeued, 3u);
    EXPECT_EQ(q.stats().current_depth, 2u);
}

TEST(PacketQueue, MaxDepthTracked) {
    PacketQueue q{100};

    for (int i = 0; i < 10; ++i) q.push(make_pkt());
    EXPECT_EQ(q.stats().max_depth, 10u);

    for (int i = 0; i < 5; ++i) q.pop();
    EXPECT_EQ(q.stats().max_depth, 10u);

    for (int i = 0; i < 3; ++i) q.push(make_pkt());
    EXPECT_EQ(q.stats().max_depth, 10u);
}

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

TEST(PacketQueue, ShutdownUnblocksWaiter) {
    PacketQueue q{10};

    bool unblocked = false;
    std::thread waiter{[&]() {
        q.wait_for_packet(std::chrono::milliseconds{5000});
        unblocked = true;
    }};

    std::this_thread::sleep_for(std::chrono::milliseconds{50});
    EXPECT_FALSE(unblocked);

    q.shutdown();
    waiter.join();
    EXPECT_TRUE(unblocked);
}

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

    EXPECT_EQ(produced.load(), N);
    EXPECT_EQ(consumed.load(), N);
}
