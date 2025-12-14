# YAHAP (Yet Another HAP)

A modern C++20 implementation of the HomeKit Accessory Protocol (HAP) with a focus on modularity and platform independence. Supports both **HAP over IP** and **HAP over BLE** transports.

## Features

- **Dual Transport Support**: Full implementation of both HAP over IP (TCP/HTTP) and HAP over BLE
- **Complete Pairing Protocol**: SRP-6a based Pair-Setup and Pair-Verify as per HAP Specification R13
- **Secure Sessions**: ChaCha20-Poly1305 encrypted communication with session key derivation
- **Platform Agnostic**: Clean abstraction layer allows porting to any platform
- **Modern C++20**: Uses `std::span`, concepts, and modern language features
- **No Exceptions/RTTI Required**: Works on embedded platforms with limited C++ runtime

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│                   Application Layer                     │
│   (Your accessory logic: Lightbulb, Sensor, etc.)       │
└─────────────────────────┬───────────────────────────────┘
                          │
┌─────────────────────────▼───────────────────────────────┐
│                   HAP Library Core                      │
│  ┌─────────────┐  ┌──────────────┐  ┌────────────────┐  │
│  │ Accessory   │  │  Pairing     │  │  Transport     │  │
│  │ Server      │  │  PairSetup   │  │  HTTP/BLE      │  │
│  │             │  │  PairVerify  │  │  SecureSession │  │
│  └─────────────┘  └──────────────┘  └────────────────┘  │
└─────────────────────────┬───────────────────────────────┘
                          │
┌─────────────────────────▼───────────────────────────────┐
│              Platform Abstraction Layer (PAL)           │
│  ┌────────┐ ┌─────────┐ ┌─────────┐ ┌────────┐ ┌─────┐  │
│  │ Crypto │ │ Network │ │ Storage │ │ System │ │ BLE │  │
│  └────────┘ └─────────┘ └─────────┘ └────────┘ └─────┘  │
└─────────────────────────────────────────────────────────┘
```

### Core Components

| Component | Description |
|-----------|-------------|
| `AccessoryServer` | Main server orchestrating HAP protocol, manages accessories and connections |
| `AttributeDatabase` | HAP object model (Accessories, Services, Characteristics) with JSON serialization |
| `PairSetup` | SRP-6a based iOS pairing handshake (M1-M6 exchange) |
| `PairVerify` | Ed25519/X25519 session establishment for paired devices |
| `SecureSession` | ChaCha20-Poly1305 encrypted frame handling |
| `BleTransport` | HAP-BLE PDU fragmentation, GATT services, and advertising |

### Platform Interfaces

To port this library, implement these abstract interfaces:

| Interface | Purpose |
|-----------|---------|
| `hap::platform::Crypto` | SHA-512, HKDF, Ed25519, X25519, ChaCha20-Poly1305 |
| `hap::platform::CryptoSRP` | SRP-6a verifier generation and client proof verification |
| `hap::platform::Network` | TCP server, mDNS registration (HAP over IP only) |
| `hap::platform::Storage` | Persistent key-value storage for pairing data |
| `hap::platform::System` | Logging, timestamps, random number generation |
| `hap::platform::Ble` | GATT server, advertising, notifications (HAP over BLE only) |

## Quick Start

### Examples

| Example | Transport | Platform | Description |
|---------|-----------|----------|-------------|
| [`examples/esp32/`](examples/esp32/) | BLE | ESP-IDF | HAP-BLE lightbulb using NimBLE |
| [`examples/linux/`](examples/linux/) | IP | Linux | HAP over IP using OpenSSL and Avahi |

## Project Structure

```
horizon-hap/
├── include/hap/
│   ├── core/              # HAP object model
│   │   ├── Accessory.hpp
│   │   ├── Service.hpp
│   │   ├── Characteristic.hpp
│   │   └── TLV8.hpp
│   ├── platform/          # Abstract platform interfaces
│   │   ├── Crypto.hpp
│   │   ├── CryptoSRP.hpp
│   │   ├── Network.hpp
│   │   ├── Storage.hpp
│   │   ├── System.hpp
│   │   └── Ble.hpp
│   ├── transport/         # Protocol implementations
│   │   ├── HTTP.hpp
│   │   ├── BleTransport.hpp
│   │   ├── SecureSession.hpp
│   │   └── PairingEndpoints.hpp
│   ├── pairing/           # Pairing protocol
│   │   ├── PairSetup.hpp
│   │   └── PairVerify.hpp
│   └── AccessoryServer.hpp
├── src/                   # Implementation files
├── examples/
│   ├── esp32/             # ESP-IDF BLE example
│   └── linux/             # Linux IP example
└── tests/                 # Unit tests
```

## Build Requirements

- **CMake** 3.20+
- **C++20** compatible compiler (GCC 10+, Clang 12+)
- **nlohmann/json** (fetched automatically via CMake)

### Building Standalone (Tests)

```bash
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build
```

### Building ESP32 Example

```bash
cd examples/esp32
idf.py set-target {chip model e.g. esp32c6}
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

### Building Linux Example

```bash
cd examples/linux
cmake -S . -B build
cmake --build build
./build/hap_lightbulb_example
```

## License

This library is licensed under the [MIT License](LICENSE).
