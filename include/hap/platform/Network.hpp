#pragma once

#include <span>
#include <string_view>
#include <string>
#include <vector>
#include <utility>
#include <functional>
#include <cstdint>

namespace hap::platform {

/**
 * @brief Abstract interface for Networking (TCP/IP and mDNS).
 */
struct Network {
    virtual ~Network() = default;

    // mDNS / Bonjour
    struct MdnsService {
        std::string_view name;
        std::string_view type; // e.g., "_hap._tcp"
        uint16_t port;
        std::vector<std::pair<std::string, std::string>> txt_records;
    };
    virtual void mdns_register(const MdnsService& service) = 0;
    virtual void mdns_update_txt_record(const MdnsService& service) = 0;

    // TCP Server
    using ConnectionId = uint32_t;
    using ReceiveCallback = std::function<void(ConnectionId id, std::span<const uint8_t> data)>;
    using DisconnectCallback = std::function<void(ConnectionId id)>;

    virtual void tcp_listen(uint16_t port, ReceiveCallback on_receive, DisconnectCallback on_disconnect) = 0;
    virtual void tcp_send(ConnectionId id, std::span<const uint8_t> data) = 0;
    virtual void tcp_disconnect(ConnectionId id) = 0;
};

} // namespace hap::platform
