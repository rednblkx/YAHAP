#include <esp_event.h>
#include <nvs_flash.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "Esp32Platform.hpp"
#include "Esp32Crypto.hpp"
#include "Esp32Ble.hpp"

#include "hap/AccessoryServer.hpp"
#include "hap/transport/BleTransport.hpp"
#include "hap/core/Accessory.hpp"
#include "hap/core/Service.hpp"
#include "hap/core/Characteristic.hpp"

static const char* TAG = "HAP_Main";

extern "C" void app_main() {
    ESP_LOGI(TAG, "Starting HAP ESP32 Example...");

    // Default Event Loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Platform Interfaces
    static Esp32System system_impl;
    static Esp32Storage storage_impl;
    static Esp32Crypto crypto_impl;
    static Esp32Ble ble_impl;

    ESP_LOGI(TAG, "Initializing BLE...");
    ble_impl.init();

    // Create Accessory Server
    hap::AccessoryServer::Config config;
    config.system = &system_impl;
    config.storage = &storage_impl;
    config.crypto = &crypto_impl;
    config.ble = &ble_impl;
    config.network = nullptr; // BLE only
    
    config.device_name = "ESP32-Lightbulb-1";
    config.accessory_id = "11:64:46:32:20:24";
    config.setup_code = "111-22-333";

    static hap::AccessoryServer server(std::move(config));

    // Create Accessory 1
    auto accessory = std::make_shared<hap::core::Accessory>(1);

    // Accessory Information Service
    auto info_service = std::make_shared<hap::core::Service>(0x3E, "Accessory Information");
    
    auto identify_char = std::make_shared<hap::core::Characteristic>(0x14, hap::core::Format::Bool, std::vector{hap::core::Permission::PairedRead, hap::core::Permission::PairedWrite});
    identify_char->set_value(false);
    identify_char->set_write_callback([](const hap::core::Value& v) {
        bool val = std::get<bool>(v);
        if (val) ESP_LOGI(TAG, "Identify!");
    });
    info_service->add_characteristic(identify_char);

    auto mfg_char = std::make_shared<hap::core::Characteristic>(0x20, hap::core::Format::String, std::vector{hap::core::Permission::PairedRead, hap::core::Permission::PairedWrite});
    mfg_char->set_value(std::string("Espressif"));
    info_service->add_characteristic(mfg_char);
    
    auto model_char = std::make_shared<hap::core::Characteristic>(0x21, hap::core::Format::String, std::vector{hap::core::Permission::PairedRead, hap::core::Permission::PairedWrite});
    model_char->set_value(std::string("ESP32-BLE-01"));
    info_service->add_characteristic(model_char);

    auto name_char = std::make_shared<hap::core::Characteristic>(0x23, hap::core::Format::String, std::vector{hap::core::Permission::PairedRead, hap::core::Permission::PairedWrite});
    name_char->set_value(std::string("ESP32 Light"));
    info_service->add_characteristic(name_char);

    auto serial_char = std::make_shared<hap::core::Characteristic>(0x30, hap::core::Format::String, std::vector{hap::core::Permission::PairedRead, hap::core::Permission::PairedWrite});
    serial_char->set_value(std::string("4923678"));
    info_service->add_characteristic(serial_char);

    auto fw_char = std::make_shared<hap::core::Characteristic>(0x52, hap::core::Format::String, std::vector{hap::core::Permission::PairedRead, hap::core::Permission::PairedWrite});
    fw_char->set_value(std::string("1.0.0"));
    info_service->add_characteristic(fw_char);

    auto hw_char = std::make_shared<hap::core::Characteristic>(0x53, hap::core::Format::String, std::vector{hap::core::Permission::PairedRead, hap::core::Permission::PairedWrite});
    hw_char->set_value(std::string("ESP32-C6"));
    info_service->add_characteristic(hw_char);

    accessory->add_service(info_service);

    // Lightbulb Service
    auto light_service = std::make_shared<hap::core::Service>(0x43, "Lightbulb");
    
    // On Characteristic - Per HAP Spec, controllable characteristics need Notify capability
    auto on_char = std::make_shared<hap::core::Characteristic>(0x25, hap::core::Format::Bool, 
        std::vector{hap::core::Permission::PairedRead, hap::core::Permission::PairedWrite, hap::core::Permission::Notify});
    on_char->set_value(false);
    on_char->set_write_callback([](const hap::core::Value& v) {
        bool on = std::get<bool>(v);
        ESP_LOGI(TAG, "Light is %s", on ? "ON" : "OFF");
    });
    light_service->add_characteristic(on_char);
    
    accessory->add_service(light_service);
    server.add_accessory(accessory);

    ESP_LOGI(TAG, "Starting Server...");
    server.start();
    
    ESP_LOGI(TAG, "Starting BLE Loop...");
    ble_impl.run();

    ESP_LOGI(TAG, "Main loop running");
}
