#include "LinuxSystem.hpp"
#include "LinuxStorage.hpp"
#include "LinuxCrypto.hpp"
#include "LinuxNetwork.hpp"
#include "hap/AccessoryServer.hpp"
#include "hap/core/Accessory.hpp"
#include "hap/core/Service.hpp"
#include "hap/core/Characteristic.hpp"

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
    std::cout << "Setup Code: 123-45-678" << std::endl;
    std::cout << "Port: 8080" << std::endl;

    // Instantiate platform interfaces
    auto system = std::make_unique<linux_pal::LinuxSystem>();
    auto storage = std::make_unique<linux_pal::LinuxStorage>("hap_storage.json");
    auto crypto = std::make_unique<linux_pal::LinuxCrypto>();
    auto network = std::make_unique<linux_pal::LinuxNetwork>();

    // Create accessory
    auto accessory = std::make_shared<hap::core::Accessory>(1);
    
    // Accessory Information Service (REQUIRED by HAP spec)
    auto info_service = std::make_shared<hap::core::Service>(0x3E, "Accessory Information");
    info_service->set_iid(1);
    
    // Name (0x23) - REQUIRED
    auto name_char = std::make_shared<hap::core::Characteristic>(
        0x23, hap::core::Format::String,
        std::vector{hap::core::Permission::PairedRead}
    );
    name_char->set_iid(2);
    name_char->set_value(std::string("HAP Lightbulb"));
    info_service->add_characteristic(name_char);
    
    // Manufacturer (0x20) - REQUIRED
    auto manufacturer_char = std::make_shared<hap::core::Characteristic>(
        0x20, hap::core::Format::String,
        std::vector{hap::core::Permission::PairedRead}
    );
    manufacturer_char->set_iid(3);
    manufacturer_char->set_value(std::string("Example Corp"));
    info_service->add_characteristic(manufacturer_char);
    
    // Model (0x21) - REQUIRED
    auto model_char = std::make_shared<hap::core::Characteristic>(
        0x21, hap::core::Format::String,
        std::vector{hap::core::Permission::PairedRead}
    );
    model_char->set_iid(4);
    model_char->set_value(std::string("HAP-Light-v1"));
    info_service->add_characteristic(model_char);
    
    // Serial Number (0x30) - REQUIRED
    auto serial_char = std::make_shared<hap::core::Characteristic>(
        0x30, hap::core::Format::String,
        std::vector{hap::core::Permission::PairedRead}
    );
    serial_char->set_iid(5);
    serial_char->set_value(std::string("00000001"));
    info_service->add_characteristic(serial_char);
    
    // Firmware Revision (0x52) - REQUIRED
    auto firmware_char = std::make_shared<hap::core::Characteristic>(
        0x52, hap::core::Format::String,
        std::vector{hap::core::Permission::PairedRead}
    );
    firmware_char->set_iid(6);
    firmware_char->set_value(std::string("1.0.0"));
    info_service->add_characteristic(firmware_char);
    
    // Identify routine
    auto identify_routine = []() {
        std::cout << "🔆 IDENTIFY routine triggered!" << std::endl;
    };

    // Identify (0x14) - REQUIRED, write-only
    auto identify_char = std::make_shared<hap::core::Characteristic>(
        0x14, hap::core::Format::Bool,
        std::vector{hap::core::Permission::PairedWrite}
    );
    identify_char->set_iid(7);
    identify_char->set_value(false);
    identify_char->set_write_callback([identify_routine](const hap::core::Value& value) {
        bool identify = std::get<bool>(value);
        if (identify) {
            identify_routine();
        }
    });
    info_service->add_characteristic(identify_char);
    
    accessory->add_service(info_service);
    
    // Lightbulb Service
    auto lightbulb_service = std::make_shared<hap::core::Service>(0x43, "Lightbulb");
    lightbulb_service->set_iid(10);
    
    // Load persisted state
    bool light_on = false;
    int32_t brightness = 100;
    
    auto on_data = storage->get("char_on");
    if (on_data && !on_data->empty()) {
        light_on = on_data->at(0) != 0;
    }
    
    auto brightness_data = storage->get("char_brightness");
    if (brightness_data && brightness_data->size() == sizeof(int32_t)) {
        std::memcpy(&brightness, brightness_data->data(), sizeof(int32_t));
    }

    auto on_char = std::make_shared<hap::core::Characteristic>(
        0x25, hap::core::Format::Bool,
        std::vector{hap::core::Permission::PairedRead, 
                   hap::core::Permission::PairedWrite,
                   hap::core::Permission::Notify}
    );
    on_char->set_iid(11);
    on_char->set_value(light_on);
    on_char->set_description("Controls the lightbulb power state");
    
    // Set write callback
    auto storage_ptr = storage.get();
    on_char->set_write_callback([storage_ptr](const hap::core::Value& value) {
        bool is_on = std::get<bool>(value);
        std::cout << "💡 Lightbulb turned " << (is_on ? "ON" : "OFF") << std::endl;
        
        uint8_t byte = is_on ? 1 : 0;
        storage_ptr->set("char_on", std::span(&byte, 1));
    });
    
    lightbulb_service->add_characteristic(on_char);
    
    // Brightness characteristic with metadata
    auto brightness_char = std::make_shared<hap::core::Characteristic>(
        0x8, hap::core::Format::Int,  // Brightness characteristic
        std::vector{hap::core::Permission::PairedRead, 
                   hap::core::Permission::PairedWrite,
                   hap::core::Permission::Notify}
    );
    brightness_char->set_iid(12);
    brightness_char->set_value(brightness);
    brightness_char->set_description("Lightbulb brightness level");
    brightness_char->set_unit("percentage");
    brightness_char->set_min_value(0.0);
    brightness_char->set_max_value(100.0);
    brightness_char->set_min_step(1.0);
    brightness_char->set_write_callback([storage_ptr](const hap::core::Value& value) {
        int32_t level = std::get<int32_t>(value);
        std::cout << "💡 Brightness set to " << level << "%" << std::endl;
        
        storage_ptr->set("char_brightness", std::span(reinterpret_cast<const uint8_t*>(&level), sizeof(level)));
    });
    
    lightbulb_service->add_characteristic(brightness_char);
    accessory->add_service(lightbulb_service);
    
    // Initialize Server
    hap::AccessoryServer::Config config;
    config.crypto = crypto.get();
    config.network = network.get();
    config.storage = storage.get();
    config.system = system.get();
    config.accessory_id = "AA:BB:CC:DD:EE:F2"; // Example ID
    config.setup_code = "123-45-678";
    config.device_name = "HAP Lightbulb";
    config.port = 8080;
    config.on_identify = identify_routine;
    
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
