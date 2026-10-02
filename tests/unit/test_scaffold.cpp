// tests/unit/test_scaffold.cpp
//
// Phase 0 scaffold test.
//
// Purpose:
//   Verify that:
//   (1) CMake finds and compiles GoogleTest correctly.
//   (2) CTest runs at least one test successfully.
//   (3) The netbucket_core library links without errors.
//
// This test is deliberately trivial.
// When Phase 1 begins, test_token_bucket.cpp will replace this as the
// primary test file.
//
// Do NOT add real logic here.

#include <gtest/gtest.h>

// ─── Scaffold: build system sanity checks ─────────────────────────────────

TEST(Scaffold, GoogleTestWorks) {
    // If this runs, CMake + GoogleTest are wired correctly.
    EXPECT_EQ(1 + 1, 2);
}

TEST(Scaffold, CppVersionIsAtLeast20) {
    // C++20 is required.  __cplusplus >= 202002L confirms the standard.
    EXPECT_GE(__cplusplus, 202002L)
        << "Compiler must be configured for C++20 or later.";
}

TEST(Scaffold, StdChronoSteadyClockIsAvailable) {
    // std::chrono::steady_clock is used for all token refill timing.
    // Verify it compiles and returns a non-zero time point.
    auto t = std::chrono::steady_clock::now().time_since_epoch().count();
    EXPECT_GT(t, 0);
}
