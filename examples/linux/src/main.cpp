#include "LinuxSystem.hpp"
#include "LinuxStorage.hpp"
#include "LinuxCrypto.hpp"
#include "LinuxNetwork.hpp"
#include "hap/AccessoryServer.hpp"
#include "hap/core/Accessory.hpp"
#include "hap/core/Service.hpp"
#include "hap/core/Characteristic.hpp"
#include "hap/core/TLV8.hpp"
#include "hap/types/ServiceTypes.hpp"

#include <cereal/cereal.hpp>
#include <cereal/archives/binary.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/utility.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/complex.hpp>
#include <cereal/types/base_class.hpp>
#include <cereal/types/array.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/map.hpp>
#include <chrono>
#include <cstdint>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <iostream>
#include <mbedtls/sha256.h>
#include <memory>
#include <unistd.h>
#include "HK_HomeKit.h"

// Global server for signal handling
std::unique_ptr<hap::AccessoryServer> g_server;
std::unique_ptr<linux_pal::LinuxStorage> storage;
readerData_t readerData;

template <class Archive>
void serialize(Archive& archive, hkEndpoint_t& endpoint) {
  archive(CEREAL_NVP(endpoint.endpoint_id), CEREAL_NVP(endpoint.last_used_at), CEREAL_NVP(endpoint.counter), CEREAL_NVP(endpoint.key_type), CEREAL_NVP(endpoint.endpoint_pk), CEREAL_NVP(endpoint.endpoint_pk_x), CEREAL_NVP(endpoint.endpoint_prst_k));
}

template <class Archive>
void serialize(Archive& archive, hkIssuer_t& issuer) {
    archive(CEREAL_NVP(issuer.issuer_id), CEREAL_NVP(issuer.issuer_pk), CEREAL_NVP(issuer.issuer_pk_x), CEREAL_NVP(issuer.endpoints));
}

template <class Archive>
void serialize(Archive& archive, readerData_t& data) {
    archive(CEREAL_NVP(data.reader_sk), CEREAL_NVP(data.reader_pk), CEREAL_NVP(data.reader_pk_x), CEREAL_NVP(data.reader_gid), CEREAL_NVP(data.reader_id), CEREAL_NVP(data.issuers));
}

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
  for (const auto& issuer : readerData.issuers) {
      if (issuer.issuer_id.size() == issuerId.size() && 
          std::equal(issuer.issuer_id.begin(), issuer.issuer_id.end(), issuerId.begin())) {
          std::cout << "Issuer already exists." << std::endl;
          return false;
      }
  }

  std::cout << "Adding new issuer with ID: " << fmt::format("{:02X}", fmt::join(issuerId, "")).c_str() << std::endl;
  hkIssuer_t newIssuer;
  newIssuer.issuer_id = issuerId;
  newIssuer.issuer_pk.assign(publicKey, publicKey + 32);

  readerData.issuers.emplace_back(newIssuer);
  std::stringstream ss;
  {
    cereal::JSONOutputArchive oarchive(ss, cereal::JSONOutputArchive::Options::NoIndent());
    oarchive(readerData);
  }
  std::string str(ss.str());
  std::vector<uint8_t> data(str.begin(), str.end());
  storage->set("reader_data", data);
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

    // Load data from storage
    if (storage->has("reader_data")) {
      std::optional<std::vector<uint8_t>> data = storage->get("reader_data");
      if(data.has_value()) {
        std::stringstream ss(std::string(data->begin(), data->end()));
        cereal::JSONInputArchive iarchive(ss);
        iarchive(readerData);
        std::cout << "Loaded reader data from storage." << std::endl;
      }
    }

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

    auto accessory = std::make_shared<hap::core::Accessory>(1);

    // Accessory Information Service
    auto info_service = hap::service::AccessoryInformationBuilder()
        .name("HAP Bridge")
        .manufacturer("Example Corp")
        .model("HAP-Bridge-v1")
        .serial_number("00000001")
        .firmware_revision("1.0.0")
        .hardware_revision("1.0.0")
        .on_identify([]() {
          std::cout << "🔆 IDENTIFY routine triggered!" << std::endl;
        })
        .hardwareFinish(hap::core::TLV8::encode({hap::core::TLV(0x01,{0xce,0xd5,0xda,0x00})}))
        .build();
    accessory->add_service(info_service);

    // HAP Protocol Information Service
    auto protocol_info_service = hap::service::HAPProtocolInformationBuilder()
        .build();
    accessory->add_service(protocol_info_service);
    
    // Create Accessory using new simplified builders
    auto accessory_2 = std::make_shared<hap::core::Accessory>(2);

    // Accessory Information Service
    auto info_service_2 = hap::service::AccessoryInformationBuilder()
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
    accessory_2->add_service(info_service_2);
    // HAP Protocol Information Service
    auto protocol_info_service_2 = hap::service::HAPProtocolInformationBuilder()
        .build();
    accessory_2->add_service(protocol_info_service_2);
    std::shared_ptr<hap::core::Service> lock_service = hap::service::LockMechanismBuilder()
        .on_lock_change([&](bool locked) {
            std::cout << "🔒 Lock changed to " << (locked ? "LOCKED" : "UNLOCKED") << std::endl;
            lock_service->characteristics()[0]->set_value(locked);
        })
        .build();
    accessory_2->add_service(lock_service);
    
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
    accessory_2->add_service(lock_mgmt_service);

    // NFC Access Service
    auto nfc_access_service = hap::service::NFCAccessBuilder()
        .on_control_point([&](const std::vector<uint8_t>& tlv) {
            std::cout << "🔒 Control Point TLV: ";
            for (uint8_t b : tlv) {
                std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
            }
            std::cout << std::endl;
            std::vector<uint8_t> tlvData = tlv;
            HK_HomeKit hk(readerData, [](const readerData_t& rd) { 
              readerData = rd;
              std::stringstream ss;
              {
                cereal::JSONOutputArchive oarchive(ss, cereal::JSONOutputArchive::Options::NoIndent());
                oarchive(readerData);
              }
              std::string str(ss.str());
              std::vector<uint8_t> data(str.begin(), str.end());
              storage->set("reader_data", data);
            }, []() { return; }, tlvData);
            std::vector<uint8_t> response = hk.processResult();
            std::cout << "🔒 Response TLV: ";
            for (uint8_t b : response) {
              std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
            }
            std::cout << std::endl;
            return response;
        })
        .build();
    accessory_2->add_service(nfc_access_service);

    // Create Accessory using new simplified builders
    auto accessory_3 = std::make_shared<hap::core::Accessory>(3);

    // Accessory Information Service
    auto info_service_3 = hap::service::AccessoryInformationBuilder()
        .name("HAP Light")
        .manufacturer("Example Corp")
        .model("HAP-Light-v1")
        .serial_number("00000001")
        .firmware_revision("1.0.0")
        .hardware_revision("1.0.0")
        .on_identify([]() {
          std::cout << "🔆 IDENTIFY routine triggered!" << std::endl;
        })
        .build();
    accessory_3->add_service(info_service_3);

    // HAP Protocol Information Service
    auto protocol_info_service_3 = hap::service::HAPProtocolInformationBuilder()
        .build();
    accessory_3->add_service(protocol_info_service_3);

    auto light_builder = hap::service::LightBulbBuilder();
    light_builder.with_brightness()
        .on_change([](bool on) {
            std::cout << "💡 Light changed to " << (on ? "ON" : "OFF") << std::endl;
        })
        .on_brightness_change([](int brightness) {
            std::cout << "💡 Brightness changed to " << brightness << std::endl;
        });

    auto light_service = light_builder.build();

    accessory_3->add_service(light_service);

    g_server = std::make_unique<hap::AccessoryServer>(std::move(config));
    
    g_server->add_accessory(accessory);
    g_server->add_accessory(accessory_2);
    g_server->add_accessory(accessory_3);
    
    // Start Server
    g_server->start();
    
    std::cout << "Server started. Press Ctrl+C to exit." << std::endl;
    
    // Keep running
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        //
        // uint8_t state = std::get<uint8_t>(lock_service->characteristics()[0]->get_value());
        // uint8_t new_state = (state == 0) ? 1 : 0;
        // lock_service->characteristics()[1]->set_value(new_state);
        // lock_service->characteristics()[0]->set_value(new_state);
        g_server->tick();
    }

    return 0;
}
