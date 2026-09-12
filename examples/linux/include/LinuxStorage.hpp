#pragma once

#include "hap/platform/Storage.hpp"
#include <nlohmann/json.hpp>
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
        
        // Convert to hex string for JSON storage
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
        
        // Convert from hex string
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
            try {
                nlohmann::json j;
                file >> j;
                data_ = j.get<std::map<std::string, std::string>>();
            } catch (...) {
                // Invalid JSON: start fresh, but say so — silent corruption
                // would silently unpair the accessory.
                std::cerr << "[LinuxStorage] Corrupt or unreadable storage file '"
                          << filename_ << "', starting fresh" << std::endl;
                data_.clear();
            }
        }
    }

    void save() {
        // Write to a temp file then rename: a crash mid-write must not
        // corrupt the live pairing data.
        std::string tmp = filename_ + ".tmp";
        {
            std::ofstream file(tmp);
            if (!file.is_open()) {
                std::cerr << "[LinuxStorage] Cannot open " << tmp << " for writing" << std::endl;
                return;
            }
            nlohmann::json j = data_;
            file << j.dump(2);
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
