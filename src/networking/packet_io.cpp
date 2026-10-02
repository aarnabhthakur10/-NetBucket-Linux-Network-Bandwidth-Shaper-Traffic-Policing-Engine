// src/networking/packet_io.cpp
#include "networking/packet_io.hpp"
#include "logger/logger.hpp"

namespace netbucket {

// ─── PacketReader ─────────────────────────────────────────────────────────────

PacketReader::PacketReader(TunDevice& tun, TrafficController& controller)
    : tun_{tun}, controller_{controller}
{}

PacketReader::~PacketReader() {
    stop();
}

void PacketReader::start() {
    if (running_.exchange(true)) return;
    thread_ = std::thread{[this] { read_loop(); }};
}

void PacketReader::stop() {
    if (!running_.exchange(false)) return;
    // The TUN read() call is blocking. Closing the fd will unblock it.
    // For a clean shutdown, the TunDevice::close() should be called from main.
    if (thread_.joinable()) thread_.join();
}

void PacketReader::read_loop() {
    NB_LOG_INFO("PacketReader", "Read loop started on: " + tun_.name());

    while (running_.load(std::memory_order_relaxed)) {
        auto raw = tun_.read_packet();
        if (raw.empty()) {
            if (!running_.load()) break;
            continue;
        }

        // Parse the IP packet and submit to the traffic controller
        Packet pkt = Packet::from_bytes(std::move(raw));
        controller_.process(std::move(pkt));
    }

    NB_LOG_INFO("PacketReader", "Read loop stopped");
}

// ─── PacketWriter ─────────────────────────────────────────────────────────────

PacketWriter::PacketWriter(TunDevice& tun_out)
    : tun_out_{tun_out}
{}

void PacketWriter::write(Packet packet) {
    if (!packet.data.empty()) {
        tun_out_.write_packet(packet.data);
    }
    // Synthetic packets (from tests) have no data — silently discard
}

ForwardFn PacketWriter::as_forward_fn() {
    // Return a lambda capturing `this` that calls write().
    // The TrafficController stores this as its forward callback.
    return [this](Packet p) { write(std::move(p)); };
}

} // namespace netbucket
