#include <gtest/gtest.h>

TEST(Scaffold, GoogleTestWorks) {
    EXPECT_EQ(1 + 1, 2);
}

TEST(Scaffold, CppVersionIsAtLeast20) {
    EXPECT_GE(__cplusplus, 202002L)
        << "Compiler must be configured for C++20 or later.";
}

TEST(Scaffold, StdChronoSteadyClockIsAvailable) {
    auto t = std::chrono::steady_clock::now().time_since_epoch().count();
    EXPECT_GT(t, 0);
}
