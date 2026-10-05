#include "core/token_bucket.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <stdexcept>
#include <thread>

using namespace netbucket;

TEST(TokenBucket, InitialTokenCount) {
    TokenBucket tb{10'000'000.0, 1'048'576.0};
    EXPECT_DOUBLE_EQ(tb.tokens(), tb.capacity_bytes());
}

TEST(TokenBucket, ConsumeCorrectTokens) {
    TokenBucket tb{10'000'000.0, 1'000'000.0};
    const double before = tb.tokens();
    const bool ok = tb.consume(1500.0);
    EXPECT_TRUE(ok);
    EXPECT_NEAR(tb.tokens(), before - 1500.0, 1.0);
}

TEST(TokenBucket, InsufficientTokens) {
    TokenBucket tb{10'000'000.0, 1.0};
    const bool ok = tb.consume(1500.0);
    EXPECT_FALSE(ok);
    EXPECT_GE(tb.tokens(), 0.0);
}

TEST(TokenBucket, BucketRefillsOverTime) {
    const double rate_bps = 10'000'000.0;
    const double cap      = 100'000.0;
    TokenBucket tb{rate_bps, cap};

    ASSERT_TRUE(tb.consume(cap));
    EXPECT_LT(tb.tokens(), cap);

    std::this_thread::sleep_for(std::chrono::milliseconds{10});

    const bool ok = tb.consume(1.0);
    EXPECT_TRUE(ok) << "Bucket should have refilled after 10ms at 10Mbps";
}

TEST(TokenBucket, CapacityRespected) {
    TokenBucket tb{1'000'000'000.0, 1000.0};

    std::this_thread::sleep_for(std::chrono::milliseconds{50});

    tb.consume(0.0);

    EXPECT_LE(tb.tokens(), 1000.0 + 1.0)
        << "Tokens must never exceed bucket capacity";
}

TEST(TokenBucket, NoTokenOverflow) {
    TokenBucket tb{100'000'000.0, 5000.0};

    std::this_thread::sleep_for(std::chrono::milliseconds{100});

    tb.consume(1.0);
    EXPECT_LE(tb.tokens(), 5000.0 + 1.0);
}

TEST(TokenBucket, FractionalRefill) {
    TokenBucket tb{1'000'000.0, 10'000.0};

    ASSERT_TRUE(tb.consume(10'000.0));

    std::this_thread::sleep_for(std::chrono::milliseconds{10});

    const bool ok = tb.consume(1000.0);
    EXPECT_TRUE(ok) << "At 1 Mbps, 10ms should give ~1250 bytes";
}

TEST(TokenBucket, ZeroRateRejected) {
    EXPECT_THROW(
        (TokenBucket{0.0, 1'000'000.0}),
        std::invalid_argument
    );
}

TEST(TokenBucket, NegativeRateRejected) {
    EXPECT_THROW(
        (TokenBucket{-1.0, 1'000'000.0}),
        std::invalid_argument
    );
}

TEST(TokenBucket, ZeroCapacityRejected) {
    EXPECT_THROW(
        (TokenBucket{10'000'000.0, 0.0}),
        std::invalid_argument
    );
}

TEST(TokenBucket, RateChange) {
    TokenBucket tb{10'000'000.0, 1'000'000.0};

    ASSERT_TRUE(tb.consume(500'000.0));
    const double tokens_before = tb.tokens();

    tb.set_rate(5'000'000.0);
    EXPECT_NEAR(tb.tokens(), tokens_before, 100.0)
        << "set_rate() should not change existing token count";

    EXPECT_NEAR(tb.rate_bps(), 5'000'000.0, 1.0);
}

TEST(TokenBucket, CapacityReductionClampsTokens) {
    TokenBucket tb{10'000'000.0, 1'000'000.0};

    tb.set_capacity(500'000.0);

    EXPECT_LE(tb.tokens(), 500'000.0)
        << "Tokens must be clamped when capacity is reduced";
}

TEST(TokenBucket, ConcurrentConsumeIsSafe) {
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

    EXPECT_EQ(success_count.load(), N_THREADS * N_PER_THREAD);

    EXPECT_LT(tb.tokens(), 1'000'000'000.0);
}

TEST(TokenBucket, SetRateRejectsInvalid) {
    TokenBucket tb{10'000'000.0, 1'000'000.0};
    EXPECT_THROW(tb.set_rate(0.0), std::invalid_argument);
    EXPECT_THROW(tb.set_rate(-5.0), std::invalid_argument);
}
