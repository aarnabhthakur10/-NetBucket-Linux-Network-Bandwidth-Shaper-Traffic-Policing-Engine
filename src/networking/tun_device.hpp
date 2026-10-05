#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace netbucket {

class TunDevice {
public:
    explicit TunDevice(const std::string& name = "netbucket0");
    ~TunDevice();

    TunDevice(const TunDevice&)            = delete;
    TunDevice& operator=(const TunDevice&) = delete;

    TunDevice(TunDevice&&) noexcept;
    TunDevice& operator=(TunDevice&&) noexcept;

    void open();

    void close();

    [[nodiscard]] bool is_open() const { return fd_ >= 0; }

    [[nodiscard]] std::vector<uint8_t> read_packet(std::size_t max_bytes = 65536);

    bool write_packet(const std::vector<uint8_t>& data);
    bool write_packet(const uint8_t* data, std::size_t len);

    [[nodiscard]] const std::string& name() const { return name_; }
    [[nodiscard]] int                fd()   const { return fd_;   }

private:
    std::string name_;
    int         fd_ = -1;

    void configure_interface();
};

}
