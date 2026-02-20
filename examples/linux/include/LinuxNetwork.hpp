#pragma once

#include "hap/platform/Network.hpp"
#include <avahi-client/client.h>
#include <avahi-client/publish.h>
#include <avahi-common/thread-watch.h>
#include <avahi-common/error.h>
#include <avahi-common/malloc.h>
#include <vector>
#include <span>
#include <thread>
#include <atomic>
#include <mutex>

namespace linux_pal {

class LinuxNetwork : public hap::platform::Network {
public:
    LinuxNetwork();
    ~LinuxNetwork() override;

    void mdns_register(const hap::platform::Network::MdnsService& service) override;
    void mdns_update_txt_record(const hap::platform::Network::MdnsService& service) override;
    void tcp_listen(uint16_t port, hap::platform::Network::ReceiveCallback callback, hap::platform::Network::DisconnectCallback disconnect_callback) override;
    void tcp_send(uint32_t connection_id, std::span<const uint8_t> data) override;
    void tcp_disconnect(uint32_t connection_id) override;

private:
    void accept_loop(uint16_t port);
    void client_handler(int client_fd, uint32_t connection_id);
    
    static void client_callback(AvahiClient *c, AvahiClientState state, void *userdata);
    static void entry_group_callback(AvahiEntryGroup *g, AvahiEntryGroupState state, void *userdata);
    void create_services(AvahiClient *c);
    
    AvahiThreadedPoll* threaded_poll_ = nullptr;
    AvahiClient* client_ = nullptr;
    AvahiEntryGroup* group_ = nullptr;
    std::string name_;
    uint16_t port_ = 0;
    hap::platform::Network::MdnsService current_service_;
    std::atomic<bool> poll_running_{false};

    hap::platform::Network::ReceiveCallback receive_callback_;
    hap::platform::Network::DisconnectCallback disconnect_callback_;
    
    std::atomic<bool> running_{false};
    std::thread accept_thread_;
};

} // namespace linux_pal
