#pragma once

#include "hap/platform/Ble.hpp"
#include <host/ble_hs.h>
#include <host/util/util.h>
#include <vector>
#include <map>

class Esp32Ble : public hap::platform::Ble {
public:
    Esp32Ble();
    ~Esp32Ble() override = default;

    void start_advertising(const Advertisement& data, uint32_t interval_ms) override;
    void stop_advertising() override;
    void register_service(const ServiceDefinition& service) override;
    void send_notification(uint16_t connection_id, const std::string& characteristic_uuid, std::span<const uint8_t> data) override;
    void disconnect(uint16_t connection_id) override;
    void set_disconnect_callback(DisconnectCallback callback) override;

    void init();
    void run();

private:
    static int ble_gap_event(struct ble_gap_event *event, void *arg);
    static int gatt_svr_chr_access(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg);
    static int gatt_svr_dsc_access(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg);

    struct CharacteristicContext {
        std::string uuid;
        CharacteristicDefinition::ReadCallback on_read;
        CharacteristicDefinition::WriteCallback on_write;
        CharacteristicDefinition::SubscribeCallback on_subscribe;
        uint16_t val_handle;
    };

    struct DescriptorContext {
        std::string uuid;
        DescriptorDefinition::ReadCallback on_read;
        DescriptorDefinition::WriteCallback on_write;
    };

    std::vector<struct ble_gatt_svc_def*> nim_services;
    std::vector<std::vector<struct ble_gatt_chr_def>> nim_characteristics_storage;
    static std::vector<CharacteristicContext*> all_contexts;
    static std::vector<DescriptorContext*> all_descriptor_contexts;
    
    DisconnectCallback disconnect_callback_;
};
