#include "LinuxSystem.hpp"
#include "LinuxStorage.hpp"
#include "LinuxCrypto.hpp"
#include "LinuxNetwork.hpp"
#include "DdkStore.hpp"
#include "hap/AccessoryServer.hpp"
#include "hap/core/Accessory.hpp"
#include "hap/core/Service.hpp"
#include "hap/core/Characteristic.hpp"
#include "hap/core/TLV8.hpp"
#include "hap/types/ServiceTypes.hpp"

#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <mbedtls/sha256.h>
#include <memory>
#include <unistd.h>
#include "HK_HomeKit.h"

// Global server for signal handling
std::unique_ptr<hap::AccessoryServer> g_server;
std::unique_ptr<linux_pal::LinuxStorage> storage;
std::unique_ptr<linux_pal::DdkStore> ddk_store;

void signal_handler(int) {
    if (g_server) {
        g_server->stop();
    }
    exit(0);
}

/**
 * @brief Produce the HomeKit issuer identifier derived from a public key.
 *
 * Computes SHA-256 over the ASCII string "key-identifier" concatenated with the provided public key
 * and returns the first 8 bytes of the digest as the issuer identifier.
 *
 * @param key Pointer to the public key bytes (expected length: 32).
 * @param len Length of the public key in bytes (should be 32).
 * @return std::vector<uint8_t> An 8-byte vector containing the first 8 bytes of the SHA-256 digest.
 */
static inline std::vector<uint8_t> getHashIdentifier(const uint8_t* key, size_t len) {
    std::vector<unsigned char> hashable;
    std::string prefix = "key-identifier";
    hashable.insert(hashable.begin(), prefix.begin(), prefix.end());
    hashable.insert(hashable.end(), key, key + len);

    uint8_t hash[32];
    mbedtls_sha256(&hashable.front(), hashable.size(), hash, 0);

    // The issuer ID is the first 8 bytes of the hash
    return std::vector<uint8_t>{hash, hash + 8};
}

static bool addIssuerIfNotExists(const std::vector<uint8_t>& issuerId, const uint8_t* publicKey) {
  for (const auto& issuer : ddk_store->issuers()) {
      if (issuer.id.size() == issuerId.size() &&
          std::equal(issuer.id.begin(), issuer.id.end(), issuerId.begin())) {
          std::cout << "Issuer already exists." << std::endl;
          return false;
      }
  }

  std::cout << "Adding new issuer with ID: " << std::hex << std::setfill('0');
  for (uint8_t b : issuerId) std::cout << std::setw(2) << static_cast<int>(b);
  std::cout << std::dec << std::endl;
  ddk::Issuer newIssuer;
  newIssuer.id = issuerId;
  newIssuer.public_key.assign(publicKey, publicKey + 32);

  ddk_store->add_issuer(std::move(newIssuer));
  return true;
}

int main() {
    std::cout << "HAP Lightbulb Example (Linux)" << std::endl;
    std::cout << "==============================" << std::endl;
    std::cout << "Setup Code: 111-22-333" << std::endl;
    std::cout << "Port: 8080" << std::endl;

    // Instantiate platform interfaces
    auto system = std::make_unique<linux_pal::LinuxSystem>();
    storage = std::make_unique<linux_pal::LinuxStorage>("hap_storage.json");
    auto crypto = std::make_unique<linux_pal::LinuxCrypto>();
    auto network = std::make_unique<linux_pal::LinuxNetwork>();

    ddk_store = std::make_unique<linux_pal::DdkStore>(*storage);

    // Create Accessory Server
    hap::AccessoryServer::Config config;
    config.system = system.get();
    config.storage = storage.get();
    config.crypto = crypto.get();
    config.ble = nullptr; // BLE only
    config.network = network.get();

    config.device_name = "HAP-Bridge";
    config.port = 8080;
    // config.accessory_id = "11:64:46:32:20:24"; // Auto-generated if empty
    config.setup_code = "111-22-333";
    config.category_id = hap::core::AccessoryCategory::Bridge;
    config.on_pairings_changed = [](const hap::PairingEvent& event) {
        std::string type_str;
        switch (event.type) {
            case hap::PairingEventType::Added: type_str = "ADDED"; break;
            case hap::PairingEventType::Removed: type_str = "REMOVED"; break;
            case hap::PairingEventType::AllRemoved: type_str = "ALL REMOVED"; break;
        }
        std::cout << "🔐 Pairing " << type_str << ": " << event.pairing_id << std::endl;
        std::vector<uint8_t> issuer_id = getHashIdentifier(event.ltpk.data(), 32);
        if (event.type == hap::PairingEventType::Added) {
          addIssuerIfNotExists(issuer_id, event.ltpk.data());
        }
    };

    namespace chr = hap::characteristic;
    auto accessory = std::make_unique<hap::core::Accessory>(1);

    // Accessory Information Service
    hap::service::ServiceBuilder info(hap::service::kType_AccessoryInformation,
                                      "Accessory Information");
    info.add(chr::CharId::Name, "HAP Bridge")
        .add(chr::CharId::Manufacturer, "Example Corp")
        .add(chr::CharId::Model, "HAP-Bridge-v1")
        .add(chr::CharId::SerialNumber, "00000001")
        .add(chr::CharId::FirmwareRevision, "1.0.0")
        .add(chr::CharId::Identify)
        .on_write_bool([]() {
          std::cout << "🔆 IDENTIFY routine triggered!" << std::endl;
        })
        .add(chr::CharId::HardwareRevision, "1.0.0")
        .add(chr::CharId::HardwareFinish);
    accessory->add_service(info.build());

    // HAP Protocol Information Service
    accessory->add_service(
        hap::service::ServiceBuilder(hap::service::kType_HAPProtocolInformation,
                                     "Protocol Information")
            .add(chr::CharId::Version)
            .build());

    // Create Accessory using new simplified builders
    auto accessory_2 = std::make_unique<hap::core::Accessory>(2);

    // Accessory Information Service
    hap::service::ServiceBuilder info2(hap::service::kType_AccessoryInformation,
                                       "Accessory Information");
    info2.add(chr::CharId::Name, "HAP Lock")
        .add(chr::CharId::Manufacturer, "Example Corp")
        .add(chr::CharId::Model, "HAP-Lock-v1")
        .add(chr::CharId::SerialNumber, "00000001")
        .add(chr::CharId::FirmwareRevision, "1.0.0")
        .add(chr::CharId::Identify)
        .on_write_bool([]() {
          std::cout << "🔆 IDENTIFY routine triggered!" << std::endl;
        })
        .add(chr::CharId::HardwareRevision, "1.0.0");
    accessory_2->add_service(info2.build());
    // HAP Protocol Information Service
    accessory_2->add_service(
        hap::service::ServiceBuilder(hap::service::kType_HAPProtocolInformation,
                                     "Protocol Information")
            .add(chr::CharId::Version)
            .build());

    // Lock Service — HAP 8.4: LockCurrentState follows LockTargetState.
    hap::service::ServiceBuilder lock(hap::service::kType_LockMechanism, "Lock Mechanism", true);
    hap::core::Characteristic* lock_current = lock.add(chr::CharId::LockCurrentStateChar).get();
    lock.add(chr::CharId::LockTargetStateChar)
        .on_write(
                  [lock_current](const hap::core::Value& v) -> hap::core::WriteResponse {
                      auto* target = std::get_if<uint8_t>(&v);
                      if (target) lock_current->set_value(*target, hap::core::EventSource{});
                      return std::nullopt;
                  })
        .on_write_bool([](bool locked) {
            std::cout << "🔒 Lock changed to " << (locked ? "LOCKED" : "UNLOCKED") << std::endl;
        });
    accessory_2->add_service(lock.build());

    // Lock Management Service - MANDATORY for Lock Profile per HAP Spec 11.2
    accessory_2->add_service(
        hap::service::ServiceBuilder(hap::service::kType_LockManagement, "Lock Management")
            .add(chr::CharId::LockControlPoint)
            .add(chr::CharId::Version)
            .on_write_tlv(
                          [](const std::vector<uint8_t>& tlv) {
                std::cout << "🔒 Control Point TLV: ";
                for (uint8_t b : tlv) {
                    std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
                }
                std::cout << std::endl;
            })
            .build());

    // NFC Access Service
    accessory_2->add_service(
        hap::service::ServiceBuilder(hap::service::kType_NFCAccess, "NFC Access")
            .add(chr::CharId::NFCAccessControlPoint)
            .add(chr::CharId::NFCAccessSupportedConfiguration)
            .add(chr::CharId::ConfigurationState)
            .on_write_response_tlv(
                [&](const std::vector<uint8_t>& tlv) -> std::optional<hap::core::Value> {
                std::cout << "🔒 Control Point TLV: ";
                for (uint8_t b : tlv) {
                    std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
                }
                std::cout << std::endl;
                HK_HomeKit hk(*ddk_store, []() { return; }, const_cast<std::vector<uint8_t>&>(tlv));
                std::vector<uint8_t> response = hk.processResult();
                std::cout << "🔒 Response TLV: ";
                for (uint8_t b : response) {
                  std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
                }
                std::cout << std::endl;
                return response;
            })
            .build());

    // Create Accessory using new simplified builders
    auto accessory_3 = std::make_unique<hap::core::Accessory>(3);

    // Accessory Information Service
    hap::service::ServiceBuilder info3(hap::service::kType_AccessoryInformation,
                                       "Accessory Information");
    info3.add(chr::CharId::Name, "HAP Light")
        .add(chr::CharId::Manufacturer, "Example Corp")
        .add(chr::CharId::Model, "HAP-Light-v1")
        .add(chr::CharId::SerialNumber, "00000001")
        .add(chr::CharId::FirmwareRevision, "1.0.0")
        .add(chr::CharId::Identify)
        .on_write_bool([]() {
          std::cout << "🔆 IDENTIFY routine triggered!" << std::endl;
        })
        .add(chr::CharId::HardwareRevision, "1.0.0");
    accessory_3->add_service(info3.build());

    // HAP Protocol Information Service
    accessory_3->add_service(
        hap::service::ServiceBuilder(hap::service::kType_HAPProtocolInformation,
                                     "Protocol Information")
            .add(chr::CharId::Version)
            .build());

    accessory_3->add_service(
        hap::service::ServiceBuilder(hap::service::kType_LightBulb, "Lightbulb", true)
            .add(chr::CharId::On)
            .on_write_bool([](bool on) {
                std::cout << "💡 Light changed to " << (on ? "ON" : "OFF") << std::endl;
            })
            .add(chr::CharId::Brightness)
            .on_write_int([](int brightness) {
                std::cout << "💡 Brightness changed to " << brightness << std::endl;
            })
            .build());

    g_server = std::make_unique<hap::AccessoryServer>(std::move(config));

    if (!g_server->add_accessory(std::move(accessory)) ||
        !g_server->add_accessory(std::move(accessory_2)) ||
        !g_server->add_accessory(std::move(accessory_3))) {
        std::cerr << "Failed to register accessory (duplicate AID or limits exceeded)" << std::endl;
        return 1;
    }

    // Start Server
    g_server->start();

    std::cout << "Server started. Press Ctrl+C to exit." << std::endl;

    // Keep running
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        g_server->tick();
    }

    return 0;
}
