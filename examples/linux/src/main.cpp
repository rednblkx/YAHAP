#include "LinuxSystem.hpp"
#include "LinuxStorage.hpp"
#include "LinuxCrypto.hpp"
#include "LinuxNetwork.hpp"
#include "hap/AccessoryServer.hpp"
#include "hap/core/Accessory.hpp"
#include "hap/core/Service.hpp"
#include "hap/core/Characteristic.hpp"
#include "hap/types/ServiceTypes.hpp"

#include <cstdint>
#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <cstring>

// Global server for signal handling
std::unique_ptr<hap::AccessoryServer> g_server;

void signal_handler(int) {
    if (g_server) {
        g_server->stop();
    }
    exit(0);
}

int main() {
    std::cout << "HAP Lightbulb Example (Linux)" << std::endl;
    std::cout << "==============================" << std::endl;
    std::cout << "Setup Code: 111-22-333" << std::endl;
    std::cout << "Port: 8080" << std::endl;

    // Instantiate platform interfaces
    auto system = std::make_unique<linux_pal::LinuxSystem>();
    auto storage = std::make_unique<linux_pal::LinuxStorage>("hap_storage.json");
    auto crypto = std::make_unique<linux_pal::LinuxCrypto>();
    auto network = std::make_unique<linux_pal::LinuxNetwork>();

    // Create Accessory Server
    hap::AccessoryServer::Config config;
    config.system = system.get();
    config.storage = storage.get();
    config.crypto = crypto.get();
    config.ble = nullptr; // BLE only
    config.network = network.get();
    
    config.device_name = "HAP-Lock";
    config.port = 8080;
    // config.accessory_id = "11:64:46:32:20:24"; // Auto-generated if empty
    config.setup_code = "111-22-333";
    config.category_id = hap::core::AccessoryCategory::DoorLock;
    
    // Create Accessory using new simplified builders
    auto accessory = std::make_shared<hap::core::Accessory>(1);

    // Accessory Information Service
    auto info_service = hap::service::AccessoryInformationBuilder()
        .name("HAP Lock")
        .manufacturer("Example Corp")
        .model("HAP-Lock-v1")
        .serial_number("00000001")
        .firmware_revision("1.0.0")
        .hardware_revision("1.0.0")
        .on_identify([]() {
          std::cout << "🔆 IDENTIFY routine triggered!" << std::endl;
        })
        .build();
    accessory->add_service(info_service);
    std::shared_ptr<hap::core::Service> lock_service = hap::service::LockMechanismBuilder()
        .on_lock_change([&](bool locked) {
            std::cout << "🔒 Lock changed to " << (locked ? "LOCKED" : "UNLOCKED") << std::endl;
            lock_service->characteristics()[0]->set_value(locked);
        })
        .build();
    accessory->add_service(lock_service);
    
    // Lock Management Service - MANDATORY for Lock Profile per HAP Spec 11.2
    auto lock_mgmt_service = hap::service::LockManagementBuilder()
        .on_control_point([](const std::vector<uint8_t>& tlv) {
            std::cout << "🔒 Control Point TLV: ";
            for (uint8_t b : tlv) {
                std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
            }
            std::cout << std::endl;
        })
        .build();
    accessory->add_service(lock_mgmt_service);    

    g_server = std::make_unique<hap::AccessoryServer>(std::move(config));
    
    g_server->add_accessory(accessory);
    
    // Start Server
    g_server->start();
    
    std::cout << "Server started. Press Ctrl+C to exit." << std::endl;
    
    // Keep running
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}
