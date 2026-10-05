#include "config/config.hpp"
#include "controller/traffic_controller.hpp"
#include "core/packet.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>

using namespace netbucket;

class TrafficControllerTest : public ::testing::Test {
protected:
    std::atomic<int> forwarded_count{0};
    std::atomic<int> dropped_count{0};

    Config make_config(TrafficMode mode, double rate_bps = 1'000'000'000.0,
                       double cap = 1'000'000'000.0, std::size_t queue = 1000) {
        Config c;
        c.rate_bps               = rate_bps;
        c.bucket_capacity_bytes  = cap;
        c.queue_capacity_packets = queue;
        c.mode                   = mode;
        return c;
    }

    ForwardFn make_forward() {
        return [this](Packet) { ++forwarded_count; };
    }
};

TEST_F(TrafficControllerTest, AllowedPacketIsForwarded) {
    auto cfg = make_config(TrafficMode::SHAPING);
    TrafficController ctrl{cfg, make_forward()};
    ctrl.start();

    ctrl.process(Packet::make_synthetic(1500));
    std::this_thread::sleep_for(std::chrono::milliseconds{10});

    EXPECT_EQ(forwarded_count.load(), 1)
        << "With a huge bucket, a single 1500-byte packet must be forwarded";
    ctrl.stop();
}

TEST_F(TrafficControllerTest, ShapingQueuesExcess) {
    auto cfg = make_config(TrafficMode::SHAPING, 1'000'000.0, 1.0, 100);
    TrafficController ctrl{cfg, make_forward()};
    ctrl.start();

    for (int i = 0; i < 10; ++i) {
        ctrl.process(Packet::make_synthetic(1500));
    }

    const auto& s = ctrl.stats();
    EXPECT_EQ(s.packets_dropped(), 0u)
        << "Shaping mode must NOT drop packets — they go into the queue";
    EXPECT_GT(s.packets_queued(), 0u)
        << "Some packets should be in the queue";
    ctrl.stop();
}

TEST_F(TrafficControllerTest, PolicingDropsExcess) {
    auto cfg = make_config(TrafficMode::POLICING, 1'000'000.0, 1.0);
    TrafficController ctrl{cfg, make_forward()};
    ctrl.start();

    for (int i = 0; i < 10; ++i) {
        ctrl.process(Packet::make_synthetic(1500));
    }

    const auto& s = ctrl.stats();
    EXPECT_GT(s.packets_dropped(), 0u)
        << "Policing mode must drop excess packets immediately";
    EXPECT_EQ(s.packets_queued(), 0u)
        << "Policing mode must never queue packets";
    ctrl.stop();
}

TEST_F(TrafficControllerTest, ModeSwitchIsLive) {
    auto cfg = make_config(TrafficMode::SHAPING, 1'000'000.0, 100.0, 1000);
    TrafficController ctrl{cfg, make_forward()};
    ctrl.start();

    EXPECT_EQ(ctrl.mode(), TrafficMode::SHAPING);

    cfg.mode = TrafficMode::POLICING;
    ctrl.apply_config(cfg);

    EXPECT_EQ(ctrl.mode(), TrafficMode::POLICING);
    ctrl.stop();
}

TEST_F(TrafficControllerTest, StatisticsUpdated) {
    auto cfg = make_config(TrafficMode::SHAPING);
    TrafficController ctrl{cfg, make_forward()};
    ctrl.start();

    for (int i = 0; i < 5; ++i) {
        ctrl.process(Packet::make_synthetic(1000));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds{20});

    const auto& s = ctrl.stats();
    EXPECT_EQ(s.packets_received(), 5u);
    ctrl.stop();
}

TEST_F(TrafficControllerTest, ApplyConfigChangesRate) {
    auto cfg = make_config(TrafficMode::SHAPING, 10'000'000.0, 20'000'000.0);
    TrafficController ctrl{cfg, make_forward()};
    ctrl.start();

    EXPECT_NEAR(ctrl.rate_bps(), 10'000'000.0, 1.0);

    cfg.rate_bps = 20'000'000.0;
    ctrl.apply_config(cfg);

    EXPECT_NEAR(ctrl.rate_bps(), 20'000'000.0, 1.0);
    ctrl.stop();
}

TEST_F(TrafficControllerTest, GracefulShutdown) {
    auto cfg = make_config(TrafficMode::SHAPING);
    TrafficController ctrl{cfg, make_forward()};
    ctrl.start();

    ctrl.process(Packet::make_synthetic(100));

    EXPECT_NO_THROW(ctrl.stop());
}
