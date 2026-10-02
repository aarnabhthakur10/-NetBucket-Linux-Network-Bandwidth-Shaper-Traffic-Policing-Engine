# Token Bucket Algorithm

## What is a Token Bucket?

A Token Bucket is a rate-limiting algorithm that models how tokens (representing bytes)
fill a bucket at a fixed rate and are consumed by packets.

## Analogy

Imagine a physical bucket with a hole in the bottom (leaky bucket) — but inverted:
- The bucket **fills** at a constant rate (tokens per second)
- Each arriving packet **removes** tokens equal to its byte count
- If the bucket has enough tokens → packet passes
- If not → packet waits (shaping) or is dropped (policing)
- Tokens that would overflow the bucket capacity are **discarded**

## Why Token Bucket vs Leaky Bucket?

| Property | Token Bucket | Leaky Bucket |
|---|---|---|
| Allows bursts | **Yes** (up to capacity) | No |
| Long-term rate | Enforced | Enforced |
| Implementation | Simple | Simple |
| Used in | Traffic shaping, QoS | Policing |

The Token Bucket is better for real-world traffic because it allows short bursts
(e.g., a web page loading all at once) while still enforcing the long-term average rate.

## Algorithm (formal)

```
# On packet arrival at time T:

elapsed = T - last_update_time          # seconds (double)
new_tokens = elapsed × rate_bytes_sec   # bytes accumulated
tokens = min(tokens + new_tokens, capacity)
last_update_time = T

if tokens ≥ packet_size:
    tokens -= packet_size
    FORWARD(packet)
else:
    if mode == SHAPING:
        ENQUEUE(packet)
    else:  # POLICING
        DROP(packet)
```

## Lazy Refill

NetBucket uses **lazy refill**: tokens are computed only when a packet arrives,
not continuously in a background thread.

Why?
- Simpler — no background thread or wake-up timer needed
- Correct — `steady_clock` measures elapsed time accurately regardless of when it is called
- Efficient — avoids unnecessary computation between packets

## Internal Units

| Quantity | Unit | Reason |
|---|---|---|
| Token count | `double` bytes | Accurate fractional accumulation |
| Rate | `double` bytes/second | Derived from bps at startup |
| Capacity | `double` bytes | Direct comparison with packet size |
| Elapsed time | `double` seconds | `std::chrono::duration<double>` |

## Why `steady_clock`?

`std::chrono::steady_clock` is **monotonically non-decreasing**.
It never jumps backward due to NTP, DST, or system clock adjustments.

If we used `system_clock`:
- An NTP correction backward jump → negative elapsed time → negative new_tokens
- This would *decrease* the token count spontaneously — incorrect rate control

## Burst Behavior

```
rate = 10 Mbps = 1,250,000 bytes/sec

Bucket A: capacity = 1 MB
  → Max burst = 1 MB at wire speed, then capped at 10 Mbps

Bucket B: capacity = 20 MB  
  → Max burst = 20 MB at wire speed, then capped at 10 Mbps
  → After 20 seconds idle, bucket B is full and can release 20 MB instantly
```

## Example Calculation

```
Rate = 10 Mbps = 1,250,000 bytes/sec
Elapsed = 1.5 ms = 0.0015 sec

New tokens = 0.0015 × 1,250,000 = 1,875 bytes

If current_tokens = 500 bytes:
  After refill: min(500 + 1875, capacity) = 2,375 bytes

Packet arrives: size = 1500 bytes
  2,375 ≥ 1,500 → FORWARD
  tokens = 2,375 - 1,500 = 875 bytes
```
