# NetBucket

> **Token-Bucket Network Bandwidth Shaper & Traffic Policing Engine**
> Wipro Capstone Project — Systems Programming in C++20 on Linux

---


## What is NetBucket?

NetBucket is a **real** Linux network bandwidth-control system built from scratch
in C++20. It uses the **Token Bucket algorithm** to:

- **Limit** network traffic to a configured rate (e.g., 10 Mbps)
- **Allow controlled bursts** up to the configured bucket capacity
- **Shape** excess traffic by queuing it until tokens are available
- **Police** excess traffic by dropping it immediately
- **Process real IP packets** via Linux TUN virtual network interfaces
- **Classify** traffic into multiple classes with independent policies

---

## Quick Start (Ubuntu 22.04+)

### Prerequisites

```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    git \
    iperf3 \
    iproute2 \
    tcpdump
```

### Build

```bash
git clone <repo-url> netbucket
cd netbucket
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
```

### Run (Phase 0 — banner only)

```bash
./build/src/netbucket
```

### Test

```bash
cd build
ctest --output-on-failure
```

---

## Current Phase

| Phase | Status | Description |
|---|---|---|
| **0** | ✅ Complete | Repository scaffold, CMake, GoogleTest |
| **1** | 🔜 Next | TokenBucket core implementation |
| 2–22 | ⏳ Planned | See architecture document |

---

## Project Structure

```
NetBucket/
├── CMakeLists.txt          — top-level CMake
├── src/
│   ├── main.cpp            — daemon entry point
│   ├── core/               — TokenBucket, Packet, PacketQueue
│   ├── controller/         — TrafficController
│   ├── scheduler/          — Packet scheduling
│   ├── classifier/         — Traffic classification
│   ├── networking/         — TUN/TAP, PacketReader/Writer
│   ├── statistics/         — StatisticsEngine
│   ├── config/             — ConfigurationManager
│   ├── cli/                — CLI parser
│   └── logger/             — Structured logging
├── tests/
│   ├── unit/               — GoogleTest unit tests
│   ├── integration/        — End-to-end tests
│   └── performance/        — Benchmarks
├── scripts/
│   ├── setup_network.sh    — Creates veth/namespaces
│   └── cleanup_network.sh  — Destroys virtual network
├── config/
│   └── default.json        — Example configuration
└── docs/                   — Architecture, design, testing docs
```

---

## Token Bucket Algorithm

A token bucket controls bandwidth by accumulating **tokens** at a fixed rate:

```
elapsed = now() - last_update
new_tokens = elapsed × rate
tokens = min(tokens + new_tokens, capacity)

if tokens ≥ packet_size:
    tokens -= packet_size
    FORWARD packet
else:
    SHAPING → queue packet
    POLICING → drop packet
```

**Key insight:** The bucket capacity controls burst size. A large bucket allows
a burst of traffic before the rate limit kicks in. A small bucket enforces the
rate much more strictly from the start.

---

## Technology Stack

| Component | Technology | Why |
|---|---|---|
| Language | C++20 | RAII, smart pointers, `std::chrono`, `std::atomic` |
| Build | CMake ≥ 3.20 | Modern target-based build |
| Testing | GoogleTest 1.14 | Unit + integration tests |
| Config | nlohmann/json | Header-only, simple JSON parsing |
| Network | Linux TUN/TAP | User-space packet processing |
| Virtual net | veth + namespaces | Isolated test environment |
| Traffic gen | iperf3 | Real traffic measurement |

---

## Documentation

| Document | Description |
|---|---|
| [Architecture Plan](docs/architecture/overview.md) | High-level design and component relationships |
| [Token Bucket Design](docs/design/token_bucket.md) | Algorithm, units, implementation decisions |
| [Phase 0 Notes](docs/stages/phase0.md) | What was built in Phase 0 |

---

## Development Phases (summary)

| Phases | Description |
|---|---|
| 0–1 | Scaffold + TokenBucket core |
| 2–4 | Packet, Queue, Controller, CLI |
| 5–7 | Linux TUN/TAP, virtual network, iperf3 |
| 8–9 | Dynamic config, Statistics |
| 10–12 | Traffic classification, multiple buckets |
| 13–14 | Linux tc / Netlink |
| 15–17 | REST API, Dashboard, Docker |
| 18–22 | Optional kernel module, benchmarks, docs, demo |

---
## 1. Install dependencies

``` bash
sudo apt update
sudo apt install -y build-essential cmake git iperf3 iproute2 tcpdump libgtest-dev nlohmann-json3-dev
```

## 2. Clone

``` bash
git clone https://github.com/aarnabhthakur10/Wipro-Capstone-Project-SOA.git
cd Wipro-Capstone-Project-SOA
```

If already cloned:

``` bash
cd ~/Wipro-Capstone-Project-SOA
```

## 3. Build --- Terminal 1

``` bash
cd ~/Wipro-Capstone-Project-SOA
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
```

The executable is:

``` text
build/src/netbucket
```

Check it:

``` bash
ls -lh build/src/netbucket
```

## 4. Run tests --- Terminal 1

``` bash
ctest --test-dir build --output-on-failure
```

## 5. Check CLI --- Terminal 1

``` bash
./build/src/netbucket help
```

## 6. Fix network script permission --- Terminal 2

``` bash
cd ~/Wipro-Capstone-Project-SOA
chmod +x ./scripts/setup_network.sh
```

Or run it without changing permission:

``` bash
sudo bash ./scripts/setup_network.sh
```

## 7. Start NetBucket --- Terminal 1

``` bash
cd ~/Wipro-Capstone-Project-SOA
sudo ./build/src/netbucket daemon --config config/default.json
```

Leave this terminal running.

## 8. Create test network --- Terminal 2

``` bash
cd ~/Wipro-Capstone-Project-SOA
sudo ./scripts/setup_network.sh
```

Verify:

``` bash
sudo ip netns list
```

Expected namespaces:

``` text
netbucket-client
netbucket-server
```

## 9. Test connectivity --- Terminal 3

``` bash
sudo ip netns exec netbucket-client ping 192.168.100.2
```

Stop with `Ctrl+C`.

## 10. Start iperf3 server --- Terminal 4

``` bash
sudo ip netns exec netbucket-server iperf3 -s
```

Leave it running.

## 11. Generate traffic --- Terminal 5

``` bash
sudo ip netns exec netbucket-client iperf3 -c 192.168.100.2 -t 30
```

## 12. Check TUN interfaces

In another terminal:

``` bash
ip link show | grep netbucket
```

## 13. Cleanup

Stop NetBucket and iperf3 with `Ctrl+C`, then:

``` bash
cd ~/Wipro-Capstone-Project-SOA
sudo ./scripts/cleanup_network.sh
```

If cleanup permission fails:

``` bash
sudo bash ./scripts/cleanup_network.sh
```

## 14. Complete terminal layout

**Terminal 1**

``` bash
cd ~/Wipro-Capstone-Project-SOA
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/src/netbucket help
sudo ./build/src/netbucket daemon --config config/default.json
```

**Terminal 2**

``` bash
cd ~/Wipro-Capstone-Project-SOA
chmod +x ./scripts/setup_network.sh
sudo ./scripts/setup_network.sh
sudo ip netns list
```

**Terminal 3**

``` bash
sudo ip netns exec netbucket-client ping 192.168.100.2
```

**Terminal 4**

``` bash
sudo ip netns exec netbucket-server iperf3 -s
```

**Terminal 5**

``` bash
sudo ip netns exec netbucket-client iperf3 -c 192.168.100.2 -t 30
```

## Important

The current namespace/veth setup and the NetBucket TUN interfaces are
not yet fully wired into one end-to-end traffic path. Therefore the
iperf3 command above tests the namespace network, but does not by itself
prove that the traffic is being limited by the Token Bucket.

The default Token Bucket configuration is:

``` text
Rate:             10 Mbps
Bucket capacity:  20 MB
Queue capacity:   1000 packets
Mode:             shaping
```

A true end-to-end shaping demo requires routing the client traffic
through the NetBucket TUN interfaces.

## Troubleshooting

If `netbucket` is not found:

``` bash
find build -type f -name "netbucket*"
```

For the current build, use:

``` bash
./build/src/netbucket help
```

If the build fails:

``` bash
rm -rf build
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
```
