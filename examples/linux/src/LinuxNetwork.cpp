#include "LinuxNetwork.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <iostream>
#include <thread>
#include <map>
#include <mutex>
#include <cassert>

namespace linux_pal {

// Global state for this simple example
static std::map<uint32_t, int> g_connections;
static std::mutex g_mutex;
static uint32_t g_next_connection_id = 1;

LinuxNetwork::LinuxNetwork() {
}

LinuxNetwork::~LinuxNetwork() {
    running_ = false;
    
    if (threaded_poll_ && poll_running_) {
        avahi_threaded_poll_stop(threaded_poll_);
        poll_running_ = false;
    }
    if (client_) {
        avahi_client_free(client_);
        client_ = nullptr;
    }
    if (threaded_poll_) {
        avahi_threaded_poll_free(threaded_poll_);
        threaded_poll_ = nullptr;
    }
    
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        for (auto& [id, fd] : g_connections) {
            close(fd);
        }
        g_connections.clear();
    }
    
    if (accept_thread_.joinable()) {
        accept_thread_.join();
    }
}

void LinuxNetwork::mdns_register(const hap::platform::Network::MdnsService& service) {
    std::cout << "mDNS Register: " << service.name << "._hap._tcp Port: " << service.port << std::endl;
    
    current_service_ = service;
    name_ = std::string(service.name);
    port_ = service.port;
    
    if (threaded_poll_ && client_) {
        mdns_update_txt_record(service);
        return;
    }
    
    if (threaded_poll_) {
        avahi_threaded_poll_free(threaded_poll_);
        threaded_poll_ = nullptr;
    }
    
    threaded_poll_ = avahi_threaded_poll_new();
    if (!threaded_poll_) {
        std::cerr << "Failed to create threaded poll object." << std::endl;
        return;
    }
    
    int error;
    client_ = avahi_client_new(avahi_threaded_poll_get(threaded_poll_), AVAHI_CLIENT_NO_FAIL, client_callback, this, &error);
    if (!client_) {
        std::cerr << "Failed to create client: " << avahi_strerror(error) << std::endl;
        avahi_threaded_poll_free(threaded_poll_);
        threaded_poll_ = nullptr;
        return;
    }
    
    int ret = avahi_threaded_poll_start(threaded_poll_);
    if (ret < 0) {
        std::cerr << "Failed to start threaded poll: " << avahi_strerror(ret) << std::endl;
        avahi_client_free(client_);
        client_ = nullptr;
        avahi_threaded_poll_free(threaded_poll_);
        threaded_poll_ = nullptr;
        return;
    }
    poll_running_ = true;
}

void LinuxNetwork::mdns_update_txt_record(const hap::platform::Network::MdnsService& service) {
    std::cout << "mDNS Update TXT: " << service.name << std::endl;
    
    // Update the current service cache
    current_service_ = service;
    
    if (!group_) {
        std::cerr << "Cannot update TXT records: No entry group exists" << std::endl;
        return;
    }
    
    if (!threaded_poll_) {
        std::cerr << "Cannot update TXT records: No threaded poll" << std::endl;
        return;
    }
    
    if (!client_) {
        std::cerr << "Cannot update TXT records: No client" << std::endl;
        return;
    }
    
    // Lock the threaded poll to safely modify the entry group
    avahi_threaded_poll_lock(threaded_poll_);
    
    // Reset the entry group to modify it
    avahi_entry_group_reset(group_);
    
    // Convert TXT records to Avahi string list
    AvahiStringList *strlst = nullptr;
    for (const auto& kv : service.txt_records) {
        std::string record = kv.first + "=" + kv.second;
        strlst = avahi_string_list_add(strlst, record.c_str());
    }
    
    // Re-add the service with updated TXT records
    int ret = avahi_entry_group_add_service_strlst(
        group_,
        AVAHI_IF_UNSPEC,
        AVAHI_PROTO_UNSPEC,
        (AvahiPublishFlags)0,
        name_.c_str(),
        "_hap._tcp",
        nullptr,  // domain
        nullptr,  // host
        port_,
        strlst
    );
    
    avahi_string_list_free(strlst);
    
    if (ret < 0) {
        std::cerr << "Failed to re-add service with updated TXT records: " << avahi_strerror(ret) << std::endl;
        avahi_threaded_poll_unlock(threaded_poll_);
        return;
    }
    
    // Commit the changes
    ret = avahi_entry_group_commit(group_);
    if (ret < 0) {
        std::cerr << "Failed to commit TXT record update: " << avahi_strerror(ret) << std::endl;
        avahi_threaded_poll_unlock(threaded_poll_);
        return;
    }
    
    avahi_threaded_poll_unlock(threaded_poll_);
    std::cout << "TXT records updated successfully" << std::endl;
}

void LinuxNetwork::client_callback(AvahiClient *c, AvahiClientState state, void *userdata) {
    LinuxNetwork* self = static_cast<LinuxNetwork*>(userdata);
    assert(c);

    std::cout << "Avahi Client State: " << state << std::endl;

    switch (state) {
        case AVAHI_CLIENT_S_RUNNING:
            std::cout << "Avahi Client Running. Creating services..." << std::endl;
            self->create_services(c);
            break;
            
        case AVAHI_CLIENT_FAILURE:
            std::cerr << "Client failure: " << avahi_strerror(avahi_client_errno(c)) << std::endl;
            avahi_threaded_poll_quit(self->threaded_poll_);
            break;
            
        case AVAHI_CLIENT_S_COLLISION:
        case AVAHI_CLIENT_S_REGISTERING:
            if (self->group_) avahi_entry_group_reset(self->group_);
            break;
            
        case AVAHI_CLIENT_CONNECTING:
            std::cout << "Avahi Client Connecting..." << std::endl;
            break;
    }
}

void LinuxNetwork::entry_group_callback(AvahiEntryGroup *g, AvahiEntryGroupState state, void* /*userdata*/) {
    std::cout << "Avahi Entry Group State: " << state << std::endl;
    switch (state) {
        case AVAHI_ENTRY_GROUP_ESTABLISHED:
            std::cout << "Service established." << std::endl;
            break;
        case AVAHI_ENTRY_GROUP_COLLISION:
            std::cerr << "Service collision." << std::endl;
            break;
        case AVAHI_ENTRY_GROUP_FAILURE:
            std::cerr << "Entry group failure: " << avahi_strerror(avahi_client_errno(avahi_entry_group_get_client(g))) << std::endl;
            break;
        case AVAHI_ENTRY_GROUP_UNCOMMITED:
        case AVAHI_ENTRY_GROUP_REGISTERING:
            break;
    }
}

void LinuxNetwork::create_services(AvahiClient *c) {
    std::cout << "create_services called." << std::endl;
    if (!group_) {
        group_ = avahi_entry_group_new(c, entry_group_callback, this);
        if (!group_) {
            std::cerr << "avahi_entry_group_new() failed: " << avahi_strerror(avahi_client_errno(c)) << std::endl;
            return;
        }
    }
    
    if (avahi_entry_group_is_empty(group_)) {
        std::cout << "Adding service: " << name_ << std::endl;
        
        // Convert TXT records to Avahi string list
        AvahiStringList *strlst = nullptr;
        for (const auto& kv : current_service_.txt_records) {
            std::string record = kv.first + "=" + kv.second;
            strlst = avahi_string_list_add(strlst, record.c_str());
        }
        
        int ret = avahi_entry_group_add_service_strlst(
            group_,
            AVAHI_IF_UNSPEC,
            AVAHI_PROTO_UNSPEC,
            (AvahiPublishFlags)0,
            name_.c_str(),
            "_hap._tcp",
            nullptr,
            nullptr,
            port_,
            strlst
        );
        
        avahi_string_list_free(strlst);
        
        if (ret < 0) {
            std::cerr << "Failed to add service: " << avahi_strerror(ret) << std::endl;
            return;
        }
        
        ret = avahi_entry_group_commit(group_);
        if (ret < 0) {
            std::cerr << "Failed to commit entry group: " << avahi_strerror(ret) << std::endl;
            return;
        }
        std::cout << "Entry group committed." << std::endl;
    }
}

void LinuxNetwork::tcp_listen(uint16_t port, hap::platform::Network::ReceiveCallback callback, hap::platform::Network::DisconnectCallback disconnect_callback) {
    receive_callback_ = callback;
    disconnect_callback_ = disconnect_callback;
    running_ = true;
    
    accept_thread_ = std::thread([this, port]() {
        this->accept_loop(port);
    });
}

void LinuxNetwork::accept_loop(uint16_t port) {
    int server_fd = socket(AF_INET6, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket failed");
        return;
    }
    
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));
    
    sockaddr_in6 addr{};
    addr.sin6_family = AF_INET6;
    addr.sin6_port = htons(port);
    addr.sin6_addr = in6addr_any;
    
    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind failed");
        close(server_fd);
        return;
    }
    
    if (listen(server_fd, SOMAXCONN) < 0) {
        perror("listen failed");
        close(server_fd);
        return;
    }
    
    std::cout << "Listening on dual-stack (IPv4+IPv6) port " << port << std::endl;
    
    while (running_) {
        int new_socket = accept(server_fd, nullptr, nullptr);
        if (new_socket < 0) {
            if (running_) perror("accept failed");
            break;
        }
        
        uint32_t conn_id;
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            conn_id = g_next_connection_id++;
            g_connections[conn_id] = new_socket;
        }
        
        std::thread([this, new_socket, conn_id]() {
            this->client_handler(new_socket, conn_id);
        }).detach();
    }
    
    close(server_fd);
}

void LinuxNetwork::client_handler(int client_fd, uint32_t connection_id) {
    uint8_t buffer[1024];
    
    while (running_) {
        ssize_t valread = read(client_fd, buffer, 1024);
        if (valread > 0) {
            if (receive_callback_) {
                receive_callback_(connection_id, std::span<const uint8_t>(buffer, valread));
            }
        } else {
            // Disconnected or error
            break;
        }
    }
    
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_connections.erase(connection_id);
    }
    close(client_fd);
    
    if (disconnect_callback_) {
        disconnect_callback_(connection_id);
    }
}

void LinuxNetwork::tcp_send(uint32_t connection_id, std::span<const uint8_t> data) {
    int fd = -1;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (g_connections.count(connection_id)) {
            fd = g_connections[connection_id];
        }
    }
    
    if (fd != -1) {
        send(fd, data.data(), data.size(), 0);
    }
}

void LinuxNetwork::tcp_disconnect(uint32_t connection_id) {
    int fd = -1;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (g_connections.count(connection_id)) {
            fd = g_connections[connection_id];
            g_connections.erase(connection_id);
        }
    }
    
    if (fd != -1) {
        close(fd);
    }
}

} // namespace linux_pal
