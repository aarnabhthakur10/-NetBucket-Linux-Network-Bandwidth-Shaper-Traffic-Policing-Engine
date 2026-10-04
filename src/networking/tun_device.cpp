// src/networking/tun_device.cpp
//
// This file uses Linux-specific APIs:
//   - /dev/net/tun
//   - TUNSETIFF ioctl
//   - IFF_TUN / IFF_NO_PI flags
//   - fcntl for non-blocking mode
//   - ip link/addr commands via system()
//
// It will only compile on Linux.

#include "networking/tun_device.hpp"
#include "logger/logger.hpp"

#ifdef __linux__

#include <fcntl.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <stdexcept>

namespace netbucket {

//  Construction 

TunDevice::TunDevice(const std::string& name)
    : name_{name}, fd_{-1}
{}

TunDevice::~TunDevice() {
    close();
}

TunDevice::TunDevice(TunDevice&& other) noexcept
    : name_{std::move(other.name_)}, fd_{other.fd_}
{
    other.fd_ = -1;
}

TunDevice& TunDevice::operator=(TunDevice&& other) noexcept {
    if (this != &other) {
        close();
        name_  = std::move(other.name_);
        fd_    = other.fd_;
        other.fd_ = -1;
    }
    return *this;
}

//  Open 

void TunDevice::open() {
    // Step 1: Open the TUN/TAP clone device
    fd_ = ::open("/dev/net/tun", O_RDWR | O_CLOEXEC);
    if (fd_ < 0) {
        throw std::runtime_error(
            std::string("Cannot open /dev/net/tun: ") + std::strerror(errno) +
            "\nHint: run with sudo or grant CAP_NET_ADMIN"
        );
    }

    // Step 2: Configure the interface using TUNSETIFF ioctl
    //
    // ifreq is the standard Linux structure for interface requests.
    // ifr_name:  the name of the interface to create (e.g. "netbucket0")
    // ifr_flags: IFF_TUN  = TUN mode (IP packets, no Ethernet header)
    //            IFF_NO_PI = do not prepend a 4-byte "packet info" header
    //                        to each packet. Without IFF_NO_PI, each read()
    //                        would return [flags 2B][proto 2B][IP packet].
    //                        With IFF_NO_PI, read() returns raw IP bytes only.
    struct ifreq ifr{};
    std::strncpy(ifr.ifr_name, name_.c_str(), IFNAMSIZ - 1);
    ifr.ifr_flags = IFF_TUN | IFF_NO_PI;

    if (::ioctl(fd_, TUNSETIFF, &ifr) < 0) {
        const int err = errno;
        ::close(fd_);
        fd_ = -1;
        throw std::runtime_error(
            std::string("TUNSETIFF failed for '") + name_ + "': " +
            std::strerror(err)
        );
    }

    // The kernel may assign a different name (e.g. if "netbucket0" was taken)
    name_ = ifr.ifr_name;

    NB_LOG_INFO("TunDevice", "Created TUN interface: " + name_);
}

//  Close 

void TunDevice::close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
        NB_LOG_INFO("TunDevice", "Closed TUN interface: " + name_);
    }
}

// Read 

std::vector<uint8_t> TunDevice::read_packet(std::size_t max_bytes) {
    std::vector<uint8_t> buf(max_bytes);

    const ssize_t n = ::read(fd_, buf.data(), buf.size());
    if (n <= 0) {
        if (n < 0 && errno != EINTR) {
            NB_LOG_ERROR("TunDevice", std::string("read() failed: ") + std::strerror(errno));
        }
        return {};
    }

    buf.resize(static_cast<std::size_t>(n));
    return buf;
}

//  Write 

bool TunDevice::write_packet(const std::vector<uint8_t>& data) {
    return write_packet(data.data(), data.size());
}

bool TunDevice::write_packet(const uint8_t* data, std::size_t len) {
    const ssize_t n = ::write(fd_, data, len);
    if (n < 0) {
        NB_LOG_ERROR("TunDevice", std::string("write() failed: ") + std::strerror(errno));
        return false;
    }
    return static_cast<std::size_t>(n) == len;
}

} // namespace netbucket

#else
//  Non-Linux stub 
// TUN/TAP is Linux-specific. On other platforms, all methods throw.

#include <stdexcept>

namespace netbucket {

TunDevice::TunDevice(const std::string& name) : name_{name}, fd_{-1} {}
TunDevice::~TunDevice() {}
TunDevice::TunDevice(TunDevice&&) noexcept = default;
TunDevice& TunDevice::operator=(TunDevice&&) noexcept = default;

void TunDevice::open() {
    throw std::runtime_error("TUN/TAP is only supported on Linux");
}
void TunDevice::close() {}
std::vector<uint8_t> TunDevice::read_packet(std::size_t) { return {}; }
bool TunDevice::write_packet(const std::vector<uint8_t>&) { return false; }
bool TunDevice::write_packet(const uint8_t*, std::size_t) { return false; }

} // namespace netbucket
#endif
