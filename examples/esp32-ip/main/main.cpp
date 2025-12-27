#include <esp_event.h>
#include <esp_log.h>
#include <esp_pm.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <memory>
#include <nvs_flash.h>
#include <vector>

#include "Esp32Crypto.hpp"
#include "Esp32Network.hpp"
#include "Esp32Platform.hpp"
#include "Esp32Storage.hpp"

#include "hap/AccessoryServer.hpp"
#include "hap/core/Accessory.hpp"
#include "hap/types/ServiceTypes.hpp"

static const char *TAG = "HAP_Main";

static std::shared_ptr<hap::core::Service> *lock_service_ptr = nullptr;

extern "C" void app_main() {
  ESP_LOGI(TAG, "Starting HAP ESP32 IP Example...");
  ESP_LOGI(TAG, "==============================");
  ESP_LOGI(TAG, "Setup Code: 111-22-333");
  ESP_LOGI(TAG, "Port: 8080");

#if CONFIG_PM_ENABLE
  // Configure dynamic frequency scaling:
  // automatic light sleep is enabled if tickless idle support is enabled.
  esp_pm_config_t pm_config = {.max_freq_mhz = 160, // Maximum CPU frequency
                               .min_freq_mhz = 10,  // Minimum CPU frequency
#if CONFIG_FREERTOS_USE_TICKLESS_IDLE
                               .light_sleep_enable = true
#endif
  };
  ESP_ERROR_CHECK(esp_pm_configure(&pm_config));
#endif // CONFIG_PM_ENABLE
  // Initialize NVS
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  // Create default event loop
  ESP_ERROR_CHECK(esp_event_loop_create_default());

  // Initialize WiFi
  if (!Esp32Network::wifi_init(CONFIG_WIFI_SSID, CONFIG_WIFI_PASSWORD)) {
    ESP_LOGE(TAG, "WiFi initialization failed!");
    return;
  }

  // Platform Interfaces
  static Esp32System system_impl;
  static Esp32Storage storage_impl;
  static Esp32Crypto crypto_impl;
  static Esp32Network network_impl;

  hap::AccessoryServer::Config config;
  config.system = &system_impl;
  config.storage = &storage_impl;
  config.crypto = &crypto_impl;
  config.ble = nullptr; // IP transport, no BLE
  config.network = &network_impl;

  config.device_name = "ESP32-Lock-IP";
  config.port = 8080;
  // config.accessory_id = "11:64:46:32:20:24"; // Auto-generated if empty
  config.setup_code = "111-22-333";
  config.category_id = hap::core::AccessoryCategory::DoorLock;

  static hap::AccessoryServer server(std::move(config));

  auto accessory = std::make_shared<hap::core::Accessory>(1);

  // Accessory Information Service
  auto info_service = hap::service::AccessoryInformationBuilder()
                          .name("ESP32-Lock-IP")
                          .manufacturer("Example Corp")
                          .model("HAP-Lock-IP-v1")
                          .serial_number("00000001")
                          .firmware_revision("1.0.0")
                          .hardware_revision("1.0.0")
                          .on_identify([]() { ESP_LOGI(TAG, "Identify!"); })
                          .build();
  accessory->add_service(info_service);

  // Protocol Information Service
  std::shared_ptr<hap::core::Service> protocol_info_service =
      hap::service::HAPProtocolInformationBuilder().build();
  accessory->add_service(protocol_info_service);

  // Lock Service
  std::shared_ptr<hap::core::Service> lock_service =
      hap::service::LockMechanismBuilder()
          .on_lock_change([&](bool locked) {
            ESP_LOGI(TAG, "Lock is %s", locked ? "LOCKED" : "UNLOCKED");
            lock_service->characteristics()[0]->set_value(locked);
          })
          .build();
  lock_service_ptr = &lock_service;
  accessory->add_service(lock_service);

  // Lock Management Service
  auto lock_mgmt_builder = hap::service::LockManagementBuilder();
  lock_mgmt_builder
      .on_control_point([](const std::vector<uint8_t> &tlv) {
        ESP_LOGI(TAG, "Control Point TLV Received(size=%d):", tlv.size());
        ESP_LOG_BUFFER_HEX(TAG, tlv.data(), tlv.size());
      })
      .build();
  accessory->add_service(lock_mgmt_builder.build());

  server.add_accessory(accessory);

  ESP_LOGI(TAG, "Starting Server...");
  server.start();

  while (true) {
    vTaskDelay(pdMS_TO_TICKS(100));
    server.tick();
  }
}
