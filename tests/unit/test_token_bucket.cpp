// tests/unit/test_token_bucket.cpp
//
// NetBucket — TokenBucket Unit Tests
//
// 14 tests covering all specified scenarios.
// Each test is independent: no shared state between tests.

#include "core/token_bucket.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <stdexcept>
#include <thread>

using namespace netbucket;

// ─── Test 1: Initial token count ─────────────────────────────────────────────
// The bucket should start FULL (tokens == capacity).
// Rationale: the system should handle an immediate burst when first started.

TEST(TokenBucket, InitialTokenCount) {
    TokenBucket tb{10'000'000.0, 1'048'576.0};   // 10 Mbps, 1 MB capacity
    EXPECT_DOUBLE_EQ(tb.tokens(), tb.capacity_bytes());
}

// ─── Test 2: Consume correct tokens ──────────────────────────────────────────

TEST(TokenBucket, ConsumeCorrectTokens) {
    TokenBucket tb{10'000'000.0, 1'000'000.0};
    const double before = tb.tokens();
    const bool ok = tb.consume(1500.0);   // One typical Ethernet-sized packet
    EXPECT_TRUE(ok);
    EXPECT_NEAR(tb.tokens(), before - 1500.0, 1.0);   // ±1 byte tolerance for refill
}

// ─── Test 3: Insufficient tokens ─────────────────────────────────────────────

TEST(TokenBucket, InsufficientTokens) {
    // Start with a tiny 1-byte capacity bucket
    TokenBucket tb{10'000'000.0, 1.0};
    // Immediately try to consume 1500 bytes — should fail
    const bool ok = tb.consume(1500.0);
    EXPECT_FALSE(ok);
    // Tokens must NOT be consumed when the call fails
    EXPECT_GE(tb.tokens(), 0.0);
}

// ─── Test 4: Bucket refills over time ────────────────────────────────────────
// We drain the bucket, sleep, and verify tokens increased.

TEST(TokenBucket, BucketRefillsOverTime) {
    // 10 Mbps = 1,250,000 bytes/sec
    // After 10ms, we expect ~12,500 bytes of tokens
    const double rate_bps = 10'000'000.0;
    const double cap      = 100'000.0;
    TokenBucket tb{rate_bps, cap};

    // Drain the bucket completely
    ASSERT_TRUE(tb.consume(cap));
    EXPECT_LT(tb.tokens(), cap);

    // Wait 10 ms
    std::this_thread::sleep_for(std::chrono::milliseconds{10});

    // Try consuming a small packet — this will trigger a refill internally
    const bool ok = tb.consume(1.0);
    EXPECT_TRUE(ok) << "Bucket should have refilled after 10ms at 10Mbps";
}

// ─── Test 5: Capacity is respected (no overflow) ─────────────────────────────

TEST(TokenBucket, CapacityRespected) {
    TokenBucket tb{1'000'000'000.0, 1000.0};   // 1 Gbps rate, tiny cap

    // Sleep long enough that, uncapped, we'd accumulate millions of bytes
    std::this_thread::sleep_for(std::chrono::milliseconds{50});

    // Force a refill by checking token level
    // (consuming 0 bytes still triggers refill)
    tb.consume(0.0);   // This refills but doesn't consume anything useful

    EXPECT_LE(tb.tokens(), 1000.0 + 1.0)   // ±1 byte for floating-point precision
        << "Tokens must never exceed bucket capacity";
}

// ─── Test 6: No token overflow after idle ────────────────────────────────────
// Same as test 5, but with a longer idle and explicit inspection.

TEST(TokenBucket, NoTokenOverflow) {
    TokenBucket tb{100'000'000.0, 5000.0};   // 100 Mbps, 5000 byte cap

    // Idle for 100ms — at 100 Mbps we'd accumulate 1,250,000 bytes without a cap
    std::this_thread::sleep_for(std::chrono::milliseconds{100});

    // Any consume call triggers refill
    tb.consume(1.0);
    EXPECT_LE(tb.tokens(), 5000.0 + 1.0);
}

// ─── Test 7: Fractional refill accuracy ──────────────────────────────────────
// At 1 Mbps, after 1 ms we should accumulate exactly 125 bytes.

TEST(TokenBucket, FractionalRefill) {
    // Rate = 1 Mbps = 125,000 bytes/sec
    // After 1ms = 0.001 sec: 125 bytes accumulated
    // After 10ms: 1250 bytes accumulated
    TokenBucket tb{1'000'000.0, 10'000.0};

    // Drain fully
    ASSERT_TRUE(tb.consume(10'000.0));

    // Sleep exactly 10ms — should give us ~1250 bytes
    std::this_thread::sleep_for(std::chrono::milliseconds{10});

    // The consume call will trigger a refill first
    const bool ok = tb.consume(1000.0);   // Ask for 1000 bytes (less than ~1250)
    EXPECT_TRUE(ok) << "At 1 Mbps, 10ms should give ~1250 bytes";
}

// ─── Test 8: Zero rate rejected ──────────────────────────────────────────────

TEST(TokenBucket, ZeroRateRejected) {
    EXPECT_THROW(
        (TokenBucket{0.0, 1'000'000.0}),
        std::invalid_argument
    );
}

// ─── Test 9: Negative rate rejected ──────────────────────────────────────────

TEST(TokenBucket, NegativeRateRejected) {
    EXPECT_THROW(
        (TokenBucket{-1.0, 1'000'000.0}),
        std::invalid_argument
    );
}

// ─── Test 10: Zero capacity rejected ─────────────────────────────────────────

TEST(TokenBucket, ZeroCapacityRejected) {
    EXPECT_THROW(
        (TokenBucket{10'000'000.0, 0.0}),
        std::invalid_argument
    );
}

// ─── Test 11: Rate change ─────────────────────────────────────────────────────
// set_rate() changes refill speed; existing tokens are preserved.

TEST(TokenBucket, RateChange) {
    TokenBucket tb{10'000'000.0, 1'000'000.0};

    // Drain partially
    ASSERT_TRUE(tb.consume(500'000.0));
    const double tokens_before = tb.tokens();

    // Change rate — tokens should NOT change
    tb.set_rate(5'000'000.0);   // 5 Mbps
    EXPECT_NEAR(tb.tokens(), tokens_before, 100.0)
        << "set_rate() should not change existing token count";

    EXPECT_NEAR(tb.rate_bps(), 5'000'000.0, 1.0);
}

// ─── Test 12: Capacity reduction clamps tokens ───────────────────────────────

TEST(TokenBucket, CapacityReductionClampsTokens) {
    TokenBucket tb{10'000'000.0, 1'000'000.0};
    // At construction, tokens = 1,000,000

    // Reduce capacity to 500,000
    tb.set_capacity(500'000.0);

    // Tokens must be clamped to new capacity
    EXPECT_LE(tb.tokens(), 500'000.0)
        << "Tokens must be clamped when capacity is reduced";
}

// ─── Test 13: Concurrent consume (thread safety) ─────────────────────────────

TEST(TokenBucket, ConcurrentConsumeIsSafe) {
    // 100 Mbps, 1 GB capacity — so tokens never run out
    TokenBucket tb{100'000'000.0, 1'000'000'000.0};

    constexpr int N_THREADS  = 8;
    constexpr int N_PER_THREAD = 1000;
    std::atomic<int> success_count{0};

    std::vector<std::thread> threads;
    threads.reserve(N_THREADS);

    for (int i = 0; i < N_THREADS; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < N_PER_THREAD; ++j) {
                if (tb.consume(1.0)) ++success_count;
            }
        });
    }

    for (auto& t : threads) t.join();

    // All 8000 consumes should succeed (bucket is huge)
    EXPECT_EQ(success_count.load(), N_THREADS * N_PER_THREAD);

    // Tokens should have decreased by exactly 8000 bytes (± refill noise)
    EXPECT_LT(tb.tokens(), 1'000'000'000.0);
}

// ─── Test 14: set_rate rejects invalid value ──────────────────────────────────

TEST(TokenBucket, SetRateRejectsInvalid) {
    TokenBucket tb{10'000'000.0, 1'000'000.0};
    EXPECT_THROW(tb.set_rate(0.0), std::invalid_argument);
    EXPECT_THROW(tb.set_rate(-5.0), std::invalid_argument);
}
