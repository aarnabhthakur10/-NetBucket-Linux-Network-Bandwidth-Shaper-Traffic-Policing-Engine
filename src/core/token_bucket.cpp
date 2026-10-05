#include "core/token_bucket.hpp"

#include <algorithm>
#include <sstream>
#include <stdexcept>

namespace netbucket {

static void validate_rate(double rate_bps) {
    if (rate_bps <= 0.0) {
        throw std::invalid_argument(
            "TokenBucket: rate_bps must be > 0, got " + std::to_string(rate_bps)
        );
    }
}

static void validate_capacity(double capacity_bytes) {
    if (capacity_bytes <= 0.0) {
        throw std::invalid_argument(
            "TokenBucket: capacity_bytes must be > 0, got " +
            std::to_string(capacity_bytes)
        );
    }
}

TokenBucket::TokenBucket(double rate_bps, double capacity_bytes)
    : rate_bps_{0.0}
    , rate_bytes_sec_{0.0}
    , capacity_bytes_{0.0}
    , tokens_{0.0}
    , last_update_{Clock::now()}
{
    validate_rate(rate_bps);
    validate_capacity(capacity_bytes);

    rate_bps_        = rate_bps;
    rate_bytes_sec_  = rate_bps / 8.0;
    capacity_bytes_  = capacity_bytes;

    tokens_ = capacity_bytes_;
}

TokenBucket::TokenBucket(const TokenBucketConfig& cfg)
    : TokenBucket(cfg.rate_bps, cfg.capacity_bytes)
{}

TokenBucket::TokenBucket(TokenBucket&& other) noexcept
    : rate_bps_{0.0}
    , rate_bytes_sec_{0.0}
    , capacity_bytes_{0.0}
    , tokens_{0.0}
    , last_update_{}
{
    std::lock_guard lock{other.mutex_};
    rate_bps_        = other.rate_bps_;
    rate_bytes_sec_  = other.rate_bytes_sec_;
    capacity_bytes_  = other.capacity_bytes_;
    tokens_          = other.tokens_;
    last_update_     = other.last_update_;
}

TokenBucket& TokenBucket::operator=(TokenBucket&& other) noexcept {
    if (this == &other) return *this;
    std::scoped_lock locks{mutex_, other.mutex_};
    rate_bps_        = other.rate_bps_;
    rate_bytes_sec_  = other.rate_bytes_sec_;
    capacity_bytes_  = other.capacity_bytes_;
    tokens_          = other.tokens_;
    last_update_     = other.last_update_;
    return *this;
}

bool TokenBucket::consume(double bytes) {
    std::lock_guard lock{mutex_};

    refill_locked();

    if (tokens_ >= bytes) {
        tokens_ -= bytes;
        return true;
    }
    return false;
}

double TokenBucket::tokens() const {
    std::lock_guard lock{mutex_};
    return tokens_;
}

double TokenBucket::rate_bps() const {
    std::lock_guard lock{mutex_};
    return rate_bps_;
}

double TokenBucket::rate_bytes_sec() const {
    std::lock_guard lock{mutex_};
    return rate_bytes_sec_;
}

double TokenBucket::capacity_bytes() const {
    std::lock_guard lock{mutex_};
    return capacity_bytes_;
}

void TokenBucket::set_rate(double rate_bps) {
    validate_rate(rate_bps);
    std::lock_guard lock{mutex_};

    refill_locked();

    rate_bps_       = rate_bps;
    rate_bytes_sec_ = rate_bps / 8.0;
}

void TokenBucket::set_capacity(double capacity_bytes) {
    validate_capacity(capacity_bytes);
    std::lock_guard lock{mutex_};

    refill_locked();

    capacity_bytes_ = capacity_bytes;

    tokens_ = std::min(tokens_, capacity_bytes_);
}

std::string TokenBucket::status() const {
    std::lock_guard lock{mutex_};
    std::ostringstream oss;
    oss << "TokenBucket{"
        << "rate=" << (rate_bps_ / 1'000'000.0) << " Mbps"
        << ", capacity=" << (capacity_bytes_ / 1'048'576.0) << " MB"
        << ", tokens=" << (tokens_ / 1'048'576.0) << " MB"
        << "}";
    return oss.str();
}

void TokenBucket::refill_locked() {
    const TimePoint now     = Clock::now();
    const std::chrono::duration<double> elapsed = now - last_update_;
    last_update_ = now;

    const double new_tokens = elapsed.count() * rate_bytes_sec_;

    tokens_ = std::min(tokens_ + new_tokens, capacity_bytes_);
}

}
