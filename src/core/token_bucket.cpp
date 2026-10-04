// src/core/token_bucket.cpp
//
// NetBucket — Token Bucket Implementation

#include "core/token_bucket.hpp"

#include <algorithm>   // std::min
#include <sstream>
#include <stdexcept>

namespace netbucket {

//  Validation helper 

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

// Construction 

TokenBucket::TokenBucket(double rate_bps, double capacity_bytes)
    : rate_bps_{0.0}        // set properly after validation below
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

    // Start with a FULL bucket.
    // Rationale: the system should be ready to handle an immediate burst
    // when first started, rather than making the first packets wait.
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
    // Lock both to prevent data races during move
    std::scoped_lock locks{mutex_, other.mutex_};
    rate_bps_        = other.rate_bps_;
    rate_bytes_sec_  = other.rate_bytes_sec_;
    capacity_bytes_  = other.capacity_bytes_;
    tokens_          = other.tokens_;
    last_update_     = other.last_update_;
    return *this;
}

// Core operation 

bool TokenBucket::consume(double bytes) {
    std::lock_guard lock{mutex_};

    refill_locked();   // Lazy refill: compute tokens earned since last call

    if (tokens_ >= bytes) {
        tokens_ -= bytes;
        return true;   // Packet allowed
    }
    return false;      // Packet must be queued (shaping) or dropped (policing)
}

//  Inspection 

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

// Live configuration 

void TokenBucket::set_rate(double rate_bps) {
    validate_rate(rate_bps);
    std::lock_guard lock{mutex_};

    // Refill at the OLD rate first, so tokens earned up to this moment
    // are credited at the correct (old) rate before switching.
    refill_locked();

    rate_bps_       = rate_bps;
    rate_bytes_sec_ = rate_bps / 8.0;
    // tokens_ is NOT changed — already-accumulated tokens remain valid.
}

void TokenBucket::set_capacity(double capacity_bytes) {
    validate_capacity(capacity_bytes);
    std::lock_guard lock{mutex_};

    // Refill at current rate before resizing.
    refill_locked();

    capacity_bytes_ = capacity_bytes;

    // Clamp current tokens to new capacity.
    // This prevents a burst when capacity is reduced, and is correct
    // when capacity is increased (min has no effect there).
    tokens_ = std::min(tokens_, capacity_bytes_);
}

//  Status 

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

// Internal: lazy refill 

// MUST be called with mutex_ held.
void TokenBucket::refill_locked() {
    const TimePoint now     = Clock::now();
    const std::chrono::duration<double> elapsed = now - last_update_;
    last_update_ = now;

    // How many bytes worth of tokens have accumulated since the last packet?
    //   new_tokens = elapsed_seconds * rate_bytes_per_second
    //
    // Example:
    //   rate = 10 Mbps = 1,250,000 bytes/sec
    //   elapsed = 1.5 ms = 0.0015 sec
    //   new_tokens = 0.0015 * 1,250,000 = 1875 bytes
    const double new_tokens = elapsed.count() * rate_bytes_sec_;

    // Add tokens but never exceed the bucket capacity.
    // min() is the "ceiling" that prevents the bucket from overflowing.
    // Without it, a long idle period could accumulate an arbitrary burst.
    tokens_ = std::min(tokens_ + new_tokens, capacity_bytes_);
}

} // namespace netbucket
