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

  auto accessory = std::make_unique<hap::core::Accessory>(1);

  namespace chr = hap::characteristic;

  // Accessory Information Service
  hap::service::ServiceBuilder info(hap::service::kType_AccessoryInformation,
                                    "Accessory Information");
  info.add(chr::CharId::Name, "ESP32-Lock-IP")
      .add(chr::CharId::Manufacturer, "Example Corp")
      .add(chr::CharId::Model, "HAP-Lock-IP-v1")
      .add(chr::CharId::SerialNumber, "00000001")
      .add(chr::CharId::FirmwareRevision, "1.0.0")
      .add(chr::CharId::Identify)
      .on_write_bool([] { ESP_LOGI(TAG, "Identify!"); })
      .add(chr::CharId::HardwareRevision, "1.0.0");
  accessory->add_service(info.build());

  // Protocol Information Service
  accessory->add_service(
      hap::service::ServiceBuilder(hap::service::kType_HAPProtocolInformation,
                                   "Protocol Information")
          .add(chr::CharId::Version)
          .build());

  // Lock Service
  hap::service::ServiceBuilder lock(hap::service::kType_LockMechanism, "Lock Mechanism", true);
  hap::core::Characteristic* lock_current = lock.add(chr::CharId::LockCurrentStateChar).get();
  lock.add(chr::CharId::LockTargetStateChar)
      .on_write(
                [lock_current](const hap::core::Value& v) -> hap::core::WriteResponse {
                    // HAP 8.4: LockCurrentState follows LockTargetState.
                    auto* target = std::get_if<uint8_t>(&v);
                    if (target) lock_current->set_value(*target, hap::core::EventSource{});
                    return std::nullopt;
                })
      .on_write_bool([](bool locked) {
          ESP_LOGI(TAG, "Lock is %s", locked ? "LOCKED" : "UNLOCKED");
      });
  accessory->add_service(lock.build());

  // Lock Management Service
  accessory->add_service(
      hap::service::ServiceBuilder(hap::service::kType_LockManagement, "Lock Management")
          .add(chr::CharId::LockControlPoint)
          .on_write_tlv(
                        [](const std::vector<uint8_t> &tlv) {
                            ESP_LOGI(TAG, "Control Point TLV Received(size=%d):", (int)tlv.size());
                            ESP_LOG_BUFFER_HEX(TAG, tlv.data(), tlv.size());
                        })
          .add(chr::CharId::Version)
          .build());

  ESP_ERROR_CHECK(server.add_accessory(std::move(accessory)) ? ESP_OK : ESP_FAIL);

  ESP_LOGI(TAG, "Starting Server...");
  server.start();

  while (true) {
    vTaskDelay(pdMS_TO_TICKS(100));
    server.tick();
  }
}
