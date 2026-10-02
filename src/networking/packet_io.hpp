// src/networking/packet_io.hpp
//
// NetBucket — Packet Reader and Writer
//
// PacketReader:  reads raw bytes from a TUN fd, creates Packet objects,
//                feeds them into a TrafficController.
//
// PacketWriter:  receives forwarded Packet objects and writes them back
//                to a second TUN fd (the "output" side of the shaper).
//
// ─── Architecture with two TUN devices ───────────────────────────────────────
//
//   Client netns          Shaper process          Server netns
//       │                      │                       │
//      tun_in           PacketReader               tun_out
//       │         read()   │   │   write()              │
//       └──────────────────┘   └────────────────────────┘
//                    TrafficController
//
// In the simplified single-TUN architecture (Phase 5):
//   - One TUN device receives packets from the client namespace
//   - The same TUN (or a second one) writes forwarded packets to the server

#pragma once

#include "controller/traffic_controller.hpp"
#include "networking/tun_device.hpp"

#include <atomic>
#include <thread>

namespace netbucket {

// ─── PacketReader ─────────────────────────────────────────────────────────────

class PacketReader {
public:
    PacketReader(TunDevice& tun, TrafficController& controller);
    ~PacketReader();

    /// Start the reading loop in a dedicated thread.
    void start();

    /// Stop the reading loop and join the thread.
    void stop();

private:
    void read_loop();

    TunDevice&          tun_;
    TrafficController&  controller_;
    std::atomic<bool>   running_{false};
    std::thread         thread_;
};

// ─── PacketWriter ─────────────────────────────────────────────────────────────

/// PacketWriter is used as the ForwardFn injected into TrafficController.
/// It writes forwarded packets to the output TUN device.

class PacketWriter {
public:
    explicit PacketWriter(TunDevice& tun_out);

    /// This is called by TrafficController for each forwarded packet.
    void write(Packet packet);

    /// Return a ForwardFn bound to this writer (for TrafficController constructor).
    ForwardFn as_forward_fn();

private:
    TunDevice& tun_out_;
};

} // namespace netbucket
