#pragma once

#include "hap/platform/Storage.hpp"
#include "hap/common/JsonValue.hpp"
#include <cstdio>
#include <fstream>
#include <iostream>
#include <mutex>
#include <map>
#include <vector>
#include <optional>

namespace linux_pal {

class LinuxStorage : public hap::platform::Storage {
public:
    explicit LinuxStorage(std::string filename) : filename_(std::move(filename)) {
        load();
    }

    void set(std::string_view key, std::span<const uint8_t> value) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::string hex_value;
        hex_value.reserve(value.size() * 2);
        for (uint8_t byte : value) {
            char buf[3];
            snprintf(buf, sizeof(buf), "%02x", byte);
            hex_value += buf;
        }
        
        data_[std::string(key)] = hex_value;
        save();
    }

    std::optional<std::vector<uint8_t>> get(std::string_view key) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = data_.find(std::string(key));
        if (it == data_.end()) {
            return std::nullopt;
        }
        
        const std::string& hex = it->second;
        std::vector<uint8_t> result;
        result.reserve(hex.size() / 2);
        
        for (size_t i = 0; i + 1 < hex.size(); i += 2) {
            unsigned int byte = 0;
            if (sscanf(hex.c_str() + i, "%2x", &byte) != 1) {
                return std::nullopt; // corrupt entry, treat as missing
            }
            result.push_back(static_cast<uint8_t>(byte));
        }
        
        return result;
    }

    void remove(std::string_view key) override {
        std::lock_guard<std::mutex> lock(mutex_);
        data_.erase(std::string(key));
        save();
    }

    bool has(std::string_view key) override {
        std::lock_guard<std::mutex> lock(mutex_);
        return data_.count(std::string(key)) > 0;
    }

private:
    std::string filename_;
    std::map<std::string, std::string> data_;
    std::mutex mutex_;

    void load() {
        std::ifstream file(filename_);
        if (file.is_open()) {
            std::string text((std::istreambuf_iterator<char>(file)),
                             std::istreambuf_iterator<char>());
            bool error = false;
            hap::common::JsonValue j = hap::common::JsonValue::parse(text, &error);
            if (error || !j.is_object()) {
                std::cerr << "[LinuxStorage] Corrupt or unreadable storage file '"
                          << filename_ << "', starting fresh" << std::endl;
                data_.clear();
                return;
            }
            for (const auto& [key, value] : j.members()) {
                if (value.is_string()) {
                    data_[key] = value.as_string();
                }
            }
        }
    }

    void save() {
        std::string tmp = filename_ + ".tmp";
        {
            std::ofstream file(tmp);
            if (!file.is_open()) {
                std::cerr << "[LinuxStorage] Cannot open " << tmp << " for writing" << std::endl;
                return;
            }
            hap::common::JsonValue j = hap::common::JsonValue::object();
            for (const auto& [key, value] : data_) {
                j.set(key, value);
            }
            file << j.dump();
            file.flush();
            if (!file.good()) {
                std::cerr << "[LinuxStorage] Write failed for " << tmp << std::endl;
                return;
            }
        }
        std::rename(tmp.c_str(), filename_.c_str());
    }
};

} // namespace linux_pal
