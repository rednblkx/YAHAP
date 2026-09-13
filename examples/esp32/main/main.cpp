#include <esp_event.h>
#include <esp_pm.h>
#include <nvs_flash.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "Esp32Ble.hpp"
#include "Esp32Crypto.hpp"
#include "Esp32Platform.hpp"
#include "Esp32Storage.hpp"
#include "esp_log_level.h"
#include "hap/AccessoryServer.hpp"
#include "hap/transport/BleTransport.hpp"
#include "hap/core/Accessory.hpp"
#include "hap/types/ServiceTypes.hpp"

static const char* TAG = "HAP_Main";

extern "C" void app_main() {
    ESP_LOGI(TAG, "Starting HAP ESP32 Example...");

    esp_err_t nvs_ret = nvs_flash_init();
    if (nvs_ret == ESP_ERR_NVS_NO_FREE_PAGES || nvs_ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs_ret);

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

    auto accessory = std::make_unique<hap::core::Accessory>(1);

    namespace chr = hap::characteristic;

    // Accessory Information Service
    hap::service::ServiceBuilder info(hap::service::kType_AccessoryInformation,
                                      "Accessory Information");
    info.add(chr::CharId::Name, "ESP32 Light")
        .add(chr::CharId::Manufacturer, "Espressif")
        .add(chr::CharId::Model, "ESP32-BLE-01")
        .add(chr::CharId::SerialNumber, "4923678")
        .add(chr::CharId::FirmwareRevision, "1.0.0")
        .add(chr::CharId::Identify)
        .on_write_bool([]() {
            ESP_LOGI(TAG, "Identify!");
        })
        .add(chr::CharId::HardwareRevision, "ESP32-C6");
    accessory->add_service(info.build());

    // Lightbulb Service
    accessory->add_service(
        hap::service::ServiceBuilder(hap::service::kType_LightBulb, "Lightbulb", true)
            .add(chr::CharId::On)
            .on_write_bool([](bool on) {
                ESP_LOGI(TAG, "Light is %s", on ? "ON" : "OFF");
            })
            .add(chr::CharId::Brightness)
            .on_write_int([](int brightness) {
                ESP_LOGI(TAG, "Brightness is %d", brightness);
            })
            .build());

    ESP_ERROR_CHECK(server.add_accessory(std::move(accessory)) ? ESP_OK : ESP_FAIL);

    ESP_LOGI(TAG, "Starting Server...");
    server.start();
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(100));
        server.tick();
    }
    
}
