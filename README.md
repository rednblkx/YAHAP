# YAHAP

A modern C++20 implementation of the HomeKit Accessory Protocol (HAP) with a focus on modularity and platform independence.

## Overview

This library provides a clean, modular architecture for implementing HAP accessories. All platform-specific functionality is abstracted through provider interfaces, allowing you to integrate this library into any platform (ESP-IDF, Linux, embedded RTOS, etc.) by implementing the required providers.

## Architecture

The library is structured around:

- **Core HAP Logic**: Implements the HAP object model (Accessories, Services, Characteristics)
- **Platform Abstraction Layer (PAL)**: Abstract interfaces for platform-specific functionality
- **Transport Layer**: Protocol logic for HAP over IP and BLE

### Platform Provider Interfaces

To use this library, you must implement the following interfaces:

- `hap::platform::Crypto` - Cryptographic primitives (SHA-512, Ed25519, ChaCha20-Poly1305, etc.)
- `hap::platform::Network` - TCP/IP networking and mDNS
- `hap::platform::Storage` - Persistent key-value storage
- `hap::platform::System` - System utilities (time, logging, RNG)
- `hap::platform::Ble` - Bluetooth Low Energy (optional, only needed for BLE accessories)

## Usage

### 1. Include as a Component

For ESP-IDF projects, copy this directory into your `components` folder:

```
my-esp-project/
├── components/
│   └── horizon-hap/   # This library
├── main/
│   └── main.cpp       # Your application
└── CMakeLists.txt
```

### 2. Implement Platform Providers

Create concrete implementations of the provider interfaces for your platform:

```cpp
// In your platform code
class EspCrypto : public hap::platform::Crypto {
    // Implement all virtual methods using mbedtls or ESP crypto APIs
};

class EspNetwork : public hap::platform::Network {
    // Implement using ESP-IDF networking stack
};

// ... and so on
```

### 3. Create and Configure Server

```cpp
#include "hap/HAP.hpp"
#include "hap/AccessoryServer.hpp"

EspCrypto crypto;
EspNetwork network;
EspStorage storage;
EspSystem system;

hap::AccessoryServer::Config config;
config.crypto = &crypto;
config.network = &network;
config.storage = &storage;
config.system = &system;

hap::AccessoryServer server(config);

// Create an accessory
auto accessory = std::make_shared<hap::core::Accessory>(1);
auto lightbulb = std::make_shared<hap::core::Service>(0x43, "Lightbulb");
auto on_char = std::make_shared<hap::core::Characteristic>(
    0x25, // "On" characteristic
    hap::core::Format::Bool,
    std::vector{hap::core::Permission::PairedRead, 
                hap::core::Permission::PairedWrite,
                hap::core::Permission::Notify}
);

lightbulb->add_characteristic(on_char);
accessory->add_service(lightbulb);
server.add_accessory(accessory);

server.start();
```

## Build Requirements

- CMake 3.20+
- C++20 compatible compiler
- Ninja (recommended build tool)

## Building Standalone

```bash
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build
./build/tests/verify_architecture
```

## License

This library is licensed under the [MIT License](LICENSE).
