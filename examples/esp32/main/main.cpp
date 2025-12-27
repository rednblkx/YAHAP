#include <esp_event.h>
#include <esp_pm.h>
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
#include "hap/types/ServiceTypes.hpp"

static const char* TAG = "HAP_Main";

extern "C" void app_main() {
    ESP_LOGI(TAG, "Starting HAP ESP32 Example...");

#if CONFIG_PM_ENABLE
    //Configure dynamic frequency scaling:
    //automatic light sleep is enabled if tickless idle support is enabled.
    esp_pm_config_t pm_config = {
        .max_freq_mhz = 160, //Maximum CPU frequency
        .min_freq_mhz = 10,  //Minimum CPU frequency
#if CONFIG_FREERTOS_USE_TICKLESS_IDLE
        .light_sleep_enable = true
#endif
    };
    ESP_ERROR_CHECK( esp_pm_configure(&pm_config) );
#endif //CONFIG_PM_ENABLE
    // Default Event Loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Platform Interfaces
    static Esp32System system_impl;
    static Esp32Storage storage_impl;
    static Esp32Crypto crypto_impl;
    static Esp32Ble ble_impl(&storage_impl);

    hap::AccessoryServer::Config config;
    config.system = &system_impl;
    config.storage = &storage_impl;
    config.crypto = &crypto_impl;
    config.ble = &ble_impl;
    config.network = nullptr; // BLE only
    
    config.device_name = "ESP32-Lightbulb-1";
    // config.accessory_id = "11:64:46:32:20:24"; // Auto-generated if empty
    config.setup_code = "111-22-333";
    config.category_id = hap::core::AccessoryCategory::DoorLock;

    static hap::AccessoryServer server(std::move(config));

    auto accessory = std::make_shared<hap::core::Accessory>(1);

    // Accessory Information Service
    auto info_service = hap::service::AccessoryInformationBuilder()
        .name("ESP32 Light")
        .manufacturer("Espressif")
        .model("ESP32-BLE-01")
        .serial_number("4923678")
        .firmware_revision("1.0.0")
        .hardware_revision("ESP32-C6")
        .on_identify([]() {
            ESP_LOGI(TAG, "Identify!");
        })
        .build();
    accessory->add_service(info_service);

    // Lightbulb Service
    auto light_builder = hap::service::LightBulbBuilder();
    light_builder.with_brightness()
        .on_change([](bool on) {
            ESP_LOGI(TAG, "Light is %s", on ? "ON" : "OFF");
        })
        .on_brightness_change([](int brightness) {
            ESP_LOGI(TAG, "Brightness is %d", brightness);
        });
    
    auto light_service = light_builder.build();
    accessory->add_service(light_service);
    
    server.add_accessory(accessory);

    ESP_LOGI(TAG, "Starting Server...");
    server.start();
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(100));
        server.tick();
    }
    
}
