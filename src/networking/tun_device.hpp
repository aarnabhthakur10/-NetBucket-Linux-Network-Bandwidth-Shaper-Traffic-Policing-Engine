// src/networking/tun_device.hpp
//
// NetBucket — TUN Virtual Network Interface
//
// What is TUN? 
//
// TUN (network TUNnel) is a Linux kernel virtual network device.
// Unlike a physical NIC (which sends frames over a wire), a TUN device
// connects a network interface directly to a user-space process via a
// file descriptor.
//
// When the kernel routes an IP packet to the tun0 interface, instead of
// sending it out a wire, it makes the packet available for user-space
// to read() from the TUN file descriptor.
//
// Similarly, when user-space write()s raw IP packet bytes to the fd,
// the kernel receives them as if they came from the network.
//
//  TUN vs TAP 
//
// TUN:  operates at Layer 3 (IP packets — no Ethernet header)
// TAP:  operates at Layer 2 (Ethernet frames — includes MAC header)
//
// We use TUN because:
//   - We work with IP packets, not Ethernet frames
//   - TUN is simpler: no MAC address management needed
//   - iperf3, ping, and other tools produce IP traffic
//   - TUN is the standard choice for VPNs, tunnels, and traffic shapers
//
// How it works 
//
//   1. open("/dev/net/tun") → returns a file descriptor
//   2. ioctl(fd, TUNSETIFF, &ifr) → creates interface "tun0" in TUN mode
//   3. ip link set tun0 up / ip addr add ... → configure the interface
//   4. Now: read(fd, buf, len) → receives one IP packet from the network
//            write(fd, buf, len) → injects one IP packet into the kernel
//
// Permissions 
//
// Opening /dev/net/tun requires CAP_NET_ADMIN (or running as root).
// The process must be run with sudo or granted the capability.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace netbucket {

class TunDevice {
public:
    /// @param name   Interface name (e.g. "tun0") — max 15 characters
    explicit TunDevice(const std::string& name = "netbucket0");
    ~TunDevice();

    // Non-copyable — owns a file descriptor
    TunDevice(const TunDevice&)            = delete;
    TunDevice& operator=(const TunDevice&) = delete;

    // Movable
    TunDevice(TunDevice&&) noexcept;
    TunDevice& operator=(TunDevice&&) noexcept;

    //  Lifecycle 

    /// Open /dev/net/tun and create the interface.
    /// @throws std::runtime_error on failure
    void open();

    /// Close the file descriptor and bring the interface down.
    void close();

    [[nodiscard]] bool is_open() const { return fd_ >= 0; }

    //  I/O 

    /// Read one IP packet from the TUN device.
    ///
    /// Blocks until a packet is available (use a dedicated thread).
    /// Returns raw IP packet bytes (no Ethernet header — we are using TUN).
    ///
    /// @param max_bytes  Buffer size (must be >= 65535 to hold max IP packet)
    /// @returns Packet bytes, or empty vector on error/close
    [[nodiscard]] std::vector<uint8_t> read_packet(std::size_t max_bytes = 65536);

    /// Write one IP packet to the TUN device.
    ///
    /// The kernel receives the packet as if it came from the network.
    ///
    /// @param data  Raw IP packet bytes
    /// @returns true on success
    bool write_packet(const std::vector<uint8_t>& data);
    bool write_packet(const uint8_t* data, std::size_t len);

    //  Accessors 

    [[nodiscard]] const std::string& name() const { return name_; }
    [[nodiscard]] int                fd()   const { return fd_;   }

private:
    std::string name_;
    int         fd_ = -1;

    void configure_interface();   ///< ip link set + addr — called after open()
};

} // namespace netbucket
