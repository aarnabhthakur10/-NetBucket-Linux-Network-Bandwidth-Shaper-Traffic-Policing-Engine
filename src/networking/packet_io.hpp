#pragma once

#include "controller/traffic_controller.hpp"
#include "networking/tun_device.hpp"

#include <atomic>
#include <thread>

namespace netbucket {

class PacketReader {
public:
    PacketReader(TunDevice& tun, TrafficController& controller);
    ~PacketReader();

    void start();

    void stop();

private:
    void read_loop();

    TunDevice&          tun_;
    TrafficController&  controller_;
    std::atomic<bool>   running_{false};
    std::thread         thread_;
};

class PacketWriter {
public:
    explicit PacketWriter(TunDevice& tun_out);

    void write(Packet packet);

    ForwardFn as_forward_fn();

private:
    TunDevice& tun_out_;
};

}
