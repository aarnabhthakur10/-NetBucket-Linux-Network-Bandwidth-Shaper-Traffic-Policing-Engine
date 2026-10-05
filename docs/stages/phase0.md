# Phase 0 — Repository Scaffold

---

## What was built

| File | Purpose |
|---|---|
| `CMakeLists.txt` | Top-level CMake: C++20, GoogleTest, nlohmann/json |
| `src/CMakeLists.txt` | `netbucket_core` static library + `netbucket` executable |
| `src/main.cpp` | Entry point: prints banner and exits |
| `src/core/placeholder.cpp` | Empty source so library compiles in Phase 0 |
| `tests/CMakeLists.txt` | Test scaffold wired to GoogleTest + CTest |
| `tests/unit/test_scaffold.cpp` | 3 trivial tests: GoogleTest works, C++20, steady_clock |
| `config/default.json` | Example JSON configuration |
| `.gitignore` | Ignores build/, _deps/, IDE files |
| `README.md` | Project overview and quickstart |

---

## How to build

```bash
# From the repo root on Ubuntu 22.04+
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
```

Expected output (abbreviated):
```
-- Build type: Debug
...
[100%] Linking CXX executable netbucket
```

## How to run

```bash
./build/netbucket
```

Expected output:
```
╔══════════════════════════════════════════════════════╗
║              NetBucket v0.1.0-phase0              ║
║  Token-Bucket Network Bandwidth Shaper & Traffic Policing Engine  ║
╚══════════════════════════════════════════════════════╝

Phase 0: Project scaffold — no traffic processing yet.
Run `ctest` to execute unit tests.
```

## How to test

```bash
cd build
ctest --output-on-failure
```

Expected: **3/3 tests pass**

```
Test project .../build
    Start 1: Scaffold.GoogleTestWorks
1/3 Test #1: Scaffold.GoogleTestWorks ..........   Passed
    Start 2: Scaffold.CppVersionIsAtLeast20
2/3 Test #2: Scaffold.CppVersionIsAtLeast20 ....   Passed
    Start 3: Scaffold.StdChronoSteadyClockIsAvailable
3/3 Test #3: Scaffold.StdChronoSteadyClockIsAvailable   Passed

100% tests passed, 0 tests failed
```

---

## What comes next

**Phase 1: TokenBucket core**

Files to create:
- `src/core/token_bucket.hpp`
- `src/core/token_bucket.cpp`
- `tests/unit/test_token_bucket.cpp` (14 tests)

All 14 unit tests must pass before Phase 2 begins.
