// src/core/token_bucket.hpp
//
// NetBucket — Token Bucket Algorithm
//
// ─── What is a Token Bucket? ──────────────────────────────────────────────────
//
// A Token Bucket is a rate-limiting algorithm. Imagine a physical bucket
// that continuously fills with tokens at a fixed rate. Each packet that
// arrives must "pay" for passage using tokens equal to its size in bytes.
//
// Key properties:
//   - Tokens accumulate at a constant rate (bytes/second)
//   - The bucket has a maximum capacity (bytes) — this controls burst size
//   - A packet consumes tokens equal to its byte size
//   - If enough tokens are available  → packet is FORWARDED
//   - If not enough tokens exist       → packet is QUEUED (shaping)
//                                     → packet is DROPPED (policing)
//
// ─── Internal Units ───────────────────────────────────────────────────────────
//
// All token quantities are stored as `double` representing **bytes**.
//
// Why double?
//   At 10 Mbps over 1.5 ms, exactly 1875 bytes of tokens accumulate.
//   Over 0.1 ms: 125 bytes. These fractional accumulations must be tracked
//   precisely across multiple packets or the long-term rate will drift.
//
//   double has 53 bits of mantissa — accurate to ±1 byte for values up to
//   ~9 petabytes, far beyond any realistic bucket size.
//
// Why NOT integer nanosecond tokens?
//   Possible but requires careful fixed-point math everywhere.
//   double is simpler to audit, unit-test, and explain.
//
// ─── Clock Choice ─────────────────────────────────────────────────────────────
//
// std::chrono::steady_clock is used for ALL timing.
//
// Why NOT system_clock?
//   system_clock tracks wall-clock time and can jump backward due to NTP
//   synchronization or DST changes. A backward jump would produce a negative
//   elapsed_time → negative new_tokens → tokens could decrease spontaneously.
//   This is incorrect and would cause silent rate errors.
//
//   steady_clock is guaranteed monotonically non-decreasing. It never jumps
//   backward. This makes token refill calculations always correct.
//
// ─── Thread Safety ────────────────────────────────────────────────────────────
//
// TokenBucket is protected by a single std::mutex.
// All public methods lock this mutex.
//
// Why mutex rather than atomics?
//   The refill operation must update BOTH tokens_ and last_update_ atomically.
//   Two separate atomics cannot guarantee this without a compare-and-swap loop.
//   A mutex is simpler, correct, and the critical section is microseconds long.

#pragma once

#include <chrono>
#include <mutex>
#include <stdexcept>
#include <string>

namespace netbucket {

// ─── TokenBucketConfig ────────────────────────────────────────────────────────

struct TokenBucketConfig {
    double rate_bps       = 10'000'000.0;   ///< Rate in bits per second
    double capacity_bytes = 20'971'520.0;   ///< Bucket capacity in bytes (20 MB)
};

// ─── TokenBucket ──────────────────────────────────────────────────────────────

class TokenBucket {
public:
    // ── Construction ──────────────────────────────────────────────────────────

    /// Construct with explicit rate (bits/sec) and capacity (bytes).
    ///
    /// @throws std::invalid_argument if rate_bps <= 0 or capacity_bytes <= 0
    explicit TokenBucket(double rate_bps, double capacity_bytes);

    /// Construct from config struct.
    explicit TokenBucket(const TokenBucketConfig& cfg);

    // Non-copyable — each bucket owns unique state
    TokenBucket(const TokenBucket&)            = delete;
    TokenBucket& operator=(const TokenBucket&) = delete;

    // Movable
    TokenBucket(TokenBucket&&)            noexcept;
    TokenBucket& operator=(TokenBucket&&) noexcept;

    ~TokenBucket() = default;

    // ── Core operation ────────────────────────────────────────────────────────

    /// Try to consume `bytes` tokens.
    ///
    /// Internally calls refill() first (lazy refill — no background thread).
    ///
    /// @param bytes  Number of bytes the packet needs (must be > 0)
    /// @returns true  if enough tokens were available (tokens consumed, packet can go)
    /// @returns false if not enough tokens (tokens unchanged, packet must be queued/dropped)
    [[nodiscard]] bool consume(double bytes);

    /// Peek at the current token level without consuming or refilling.
    /// Primarily for status reporting and unit tests.
    [[nodiscard]] double tokens() const;

    // ── Configuration (thread-safe, live update) ──────────────────────────────

    /// Change the token refill rate at runtime.
    ///
    /// The current token level is NOT changed by this operation.
    /// Tokens already accumulated remain valid.
    /// The new rate takes effect immediately on the next refill.
    ///
    /// @throws std::invalid_argument if rate_bps <= 0
    void set_rate(double rate_bps);

    /// Change the bucket capacity at runtime.
    ///
    /// If current tokens exceed the new capacity, they are clamped to the
    /// new capacity. This prevents a sudden large burst if capacity is reduced.
    ///
    /// @throws std::invalid_argument if capacity_bytes <= 0
    void set_capacity(double capacity_bytes);

    // ── Inspection (thread-safe) ───────────────────────────────────────────────

    [[nodiscard]] double rate_bps()        const;
    [[nodiscard]] double rate_bytes_sec()  const;   ///< rate_bps / 8
    [[nodiscard]] double capacity_bytes()  const;

    /// Human-readable status string (for CLI / logging)
    [[nodiscard]] std::string status() const;

private:
    // ── Internal helpers (must be called with mutex_ held) ────────────────────

    /// Recompute token count based on elapsed time since last_update_.
    /// This is the "lazy refill" — happens only when a packet arrives,
    /// not in a background thread.
    ///
    /// Algorithm:
    ///   elapsed      = now - last_update_   (seconds, double)
    ///   new_tokens   = elapsed * rate_bytes_sec_
    ///   tokens_      = min(tokens_ + new_tokens, capacity_bytes_)
    ///   last_update_ = now
    void refill_locked();

    // ── Data members ──────────────────────────────────────────────────────────

    mutable std::mutex mutex_;

    double rate_bps_;           ///< Configured rate in bits/second
    double rate_bytes_sec_;     ///< Derived: rate_bps_ / 8.0  (bytes/second)
    double capacity_bytes_;     ///< Maximum token capacity (bytes)
    double tokens_;             ///< Current token count (bytes)

    using Clock     = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    TimePoint last_update_;     ///< When tokens_ was last recalculated
};

} // namespace netbucket
