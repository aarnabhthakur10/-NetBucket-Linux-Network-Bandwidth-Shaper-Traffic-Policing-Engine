#pragma once

#include <chrono>
#include <mutex>
#include <stdexcept>
#include <string>

namespace netbucket {

struct TokenBucketConfig {
    double rate_bps       = 10'000'000.0;
    double capacity_bytes = 20'971'520.0;
};

class TokenBucket {
public:

    explicit TokenBucket(double rate_bps, double capacity_bytes);

    explicit TokenBucket(const TokenBucketConfig& cfg);

    TokenBucket(const TokenBucket&)            = delete;
    TokenBucket& operator=(const TokenBucket&) = delete;

    TokenBucket(TokenBucket&&)            noexcept;
    TokenBucket& operator=(TokenBucket&&) noexcept;

    ~TokenBucket() = default;

    [[nodiscard]] bool consume(double bytes);

    [[nodiscard]] double tokens() const;

    void set_rate(double rate_bps);

    void set_capacity(double capacity_bytes);

    [[nodiscard]] double rate_bps()        const;
    [[nodiscard]] double rate_bytes_sec()  const;
    [[nodiscard]] double capacity_bytes()  const;

    [[nodiscard]] std::string status() const;

private:

    void refill_locked();

    mutable std::mutex mutex_;

    double rate_bps_;
    double rate_bytes_sec_;
    double capacity_bytes_;
    double tokens_;

    using Clock     = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    TimePoint last_update_;
};

}
