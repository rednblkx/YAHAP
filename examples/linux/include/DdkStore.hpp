#pragma once

#include "ddk/store/CredentialStore.h"
#include "ddk/store/Issuer.h"
#include "ddk/store/Endpoint.h"
#include "ddk/store/ReaderIdentity.h"
#include "hap/common/JsonValue.hpp"
#include "LinuxStorage.hpp"

#include <mutex>
#include <iostream>

namespace linux_pal {

// ddk::CredentialStore backed by the linux PAL storage. Serializes the
// reader identity, issuers and endpoints as JSON under the "ddk_credentials"
// storage key, using JsonValue.
class DdkStore : public ddk::CredentialStore {
public:
    explicit DdkStore(LinuxStorage& storage) : storage_(storage) {
        load();
    }

    const ddk::ReaderIdentity& reader_identity() const override {
        return identity_;
    }

    void provision_identity(const ddk::ReaderIdentity& identity) override {
        identity_ = identity;
    }

    ddk::span<ddk::Issuer> issuers() override {
        return issuers_;
    }

    void add_issuer(ddk::Issuer issuer) {
        issuers_.push_back(std::move(issuer));
        persist();
    }

    void save() override {
        std::lock_guard<std::mutex> lock(mutex_);
        persist();
    }

private:
    static hap::common::JsonValue to_json(const std::vector<uint8_t>& bytes) {
        hap::common::JsonValue arr = hap::common::JsonValue::array();
        for (uint8_t b : bytes) {
            arr.push_back(b);
        }
        return arr;
    }

    static std::vector<uint8_t> bytes_from_json(const hap::common::JsonValue* v) {
        std::vector<uint8_t> out;
        if (!v || !v->is_array()) {
            return out;
        }
        out.reserve(v->size());
        for (const auto& item : v->items()) {
            out.push_back(static_cast<uint8_t>(item.as_int()));
        }
        return out;
    }

    static hap::common::JsonValue endpoint_to_json(const ddk::Endpoint& ep) {
        hap::common::JsonValue obj = hap::common::JsonValue::object();
        obj.set("id", to_json(ep.id));
        obj.set("public_key", to_json(ep.public_key));
        obj.set("public_key_x", to_json(ep.public_key_x));
        obj.set("persistent_key", to_json(ep.persistent_key));
        obj.set("last_used_at", ep.used_at);
        obj.set("counter", ep.counter);
        obj.set("key_type", static_cast<unsigned>(ep.key_type));

        hap::common::JsonValue aliro = hap::common::JsonValue::object();
        aliro.set("key_slot", to_json(ep.aliro.key_slot));
        if (ep.aliro.signaling_bitmask) {
            aliro.set("signaling_bitmask", *ep.aliro.signaling_bitmask);
        }
        aliro.set("credential_signed_timestamp", to_json(ep.aliro.credential_signed_timestamp));
        aliro.set("revocation_signed_timestamp", to_json(ep.aliro.revocation_signed_timestamp));
        hap::common::JsonValue docs = hap::common::JsonValue::array();
        for (const auto& doc : ep.aliro.documents) {
            docs.push_back(to_json(doc));
        }
        aliro.set("documents", std::move(docs));
        aliro.set("last_flow", static_cast<int>(ep.aliro.last_flow));
        obj.set("aliro", std::move(aliro));
        return obj;
    }

    static ddk::Endpoint endpoint_from_json(const hap::common::JsonValue& obj) {
        ddk::Endpoint ep;
        ep.id = bytes_from_json(obj.find("id"));
        ep.public_key = bytes_from_json(obj.find("public_key"));
        ep.public_key_x = bytes_from_json(obj.find("public_key_x"));
        ep.persistent_key = bytes_from_json(obj.find("persistent_key"));
        ep.used_at = obj.find("last_used_at") ? obj.find("last_used_at")->as_uint32() : 0;
        ep.counter = obj.find("counter") ? static_cast<uint8_t>(obj.find("counter")->as_int()) : 0;
        ep.key_type = obj.find("key_type")
            ? static_cast<ddk::KeyType>(obj.find("key_type")->as_int())
            : ddk::KeyType::Secp256r1;

        if (const hap::common::JsonValue* aliro = obj.find("aliro")) {
            ep.aliro.key_slot = bytes_from_json(aliro->find("key_slot"));
            if (const auto* mask = aliro->find("signaling_bitmask")) {
                ep.aliro.signaling_bitmask = mask->as_uint32() & 0xFFFF;
            }
            ep.aliro.credential_signed_timestamp =
                bytes_from_json(aliro->find("credential_signed_timestamp"));
            ep.aliro.revocation_signed_timestamp =
                bytes_from_json(aliro->find("revocation_signed_timestamp"));
            if (const auto* docs = aliro->find("documents")) {
                for (const auto& doc : docs->items()) {
                    ep.aliro.documents.push_back(bytes_from_json(&doc));
                }
            }
            if (const auto* flow = aliro->find("last_flow")) {
                ep.aliro.last_flow = static_cast<ddk::KeyFlow>(flow->as_int());
            }
        }
        return ep;
    }

    static hap::common::JsonValue issuer_to_json(const ddk::Issuer& issuer) {
        hap::common::JsonValue obj = hap::common::JsonValue::object();
        obj.set("id", to_json(issuer.id));
        obj.set("public_key", to_json(issuer.public_key));
        obj.set("public_key_x", to_json(issuer.public_key_x));
        hap::common::JsonValue endpoints = hap::common::JsonValue::array();
        for (const auto& ep : issuer.endpoints) {
            endpoints.push_back(endpoint_to_json(ep));
        }
        obj.set("endpoints", std::move(endpoints));
        return obj;
    }

    static ddk::Issuer issuer_from_json(const hap::common::JsonValue& obj) {
        ddk::Issuer issuer;
        issuer.id = bytes_from_json(obj.find("id"));
        issuer.public_key = bytes_from_json(obj.find("public_key"));
        issuer.public_key_x = bytes_from_json(obj.find("public_key_x"));
        if (const auto* endpoints = obj.find("endpoints")) {
            for (const auto& ep : endpoints->items()) {
                issuer.endpoints.push_back(endpoint_from_json(ep));
            }
        }
        return issuer;
    }

    void load() {
        auto data = storage_.get("ddk_credentials");
        if (!data) {
            return;
        }
        std::string text(data->begin(), data->end());
        bool error = false;
        hap::common::JsonValue root = hap::common::JsonValue::parse(text, &error);
        if (error || !root.is_object()) {
            std::cerr << "Failed to parse DDK credentials — starting fresh"
                      << std::endl;
            return;
        }

        if (const auto* identity = root.find("identity")) {
            identity_.private_key = bytes_from_json(identity->find("private_key"));
            identity_.public_key = bytes_from_json(identity->find("public_key"));
            identity_.public_key_x = bytes_from_json(identity->find("public_key_x"));
            identity_.group_identifier = bytes_from_json(identity->find("group_identifier"));
            identity_.sub_identifier = bytes_from_json(identity->find("sub_identifier"));
        }
        if (const auto* issuers = root.find("issuers")) {
            for (const auto& issuer : issuers->items()) {
                issuers_.push_back(issuer_from_json(issuer));
            }
        }
        std::cout << "Loaded DDK credentials from storage." << std::endl;
    }

    void persist() {
        hap::common::JsonValue root = hap::common::JsonValue::object();

        hap::common::JsonValue identity = hap::common::JsonValue::object();
        identity.set("private_key", to_json(identity_.private_key));
        identity.set("public_key", to_json(identity_.public_key));
        identity.set("public_key_x", to_json(identity_.public_key_x));
        identity.set("group_identifier", to_json(identity_.group_identifier));
        identity.set("sub_identifier", to_json(identity_.sub_identifier));
        root.set("identity", std::move(identity));

        hap::common::JsonValue issuers = hap::common::JsonValue::array();
        for (const auto& issuer : issuers_) {
            issuers.push_back(issuer_to_json(issuer));
        }
        root.set("issuers", std::move(issuers));

        std::string str = root.dump();
        std::vector<uint8_t> data(str.begin(), str.end());
        storage_.set("ddk_credentials", data);
    }

    LinuxStorage& storage_;
    ddk::ReaderIdentity identity_;
    std::vector<ddk::Issuer> issuers_;
    std::mutex mutex_;
};

} // namespace linux_pal
