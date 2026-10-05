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

void TunDevice::open() {
    fd_ = ::open("/dev/net/tun", O_RDWR | O_CLOEXEC);
    if (fd_ < 0) {
        throw std::runtime_error(
            std::string("Cannot open /dev/net/tun: ") + std::strerror(errno) +
            "\nHint: run with sudo or grant CAP_NET_ADMIN"
        );
    }

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

    name_ = ifr.ifr_name;

    NB_LOG_INFO("TunDevice", "Created TUN interface: " + name_);
}

void TunDevice::close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
        NB_LOG_INFO("TunDevice", "Closed TUN interface: " + name_);
    }
}

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

}

#else

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

}
#endif
