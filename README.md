# YAHAP (Yet Another HAP)

A modern C++20 implementation of the HomeKit Accessory Protocol (HAP) with a focus on modularity and platform independence. Supports both **HAP over IP** and **HAP over BLE** transports.

## Features

- **Dual Transport Support**: Full implementation of both HAP over IP (TCP/HTTP) and HAP over BLE
- **Complete Pairing Protocol**: SRP-6a based Pair-Setup and Pair-Verify
- **Secure Sessions**: ChaCha20-Poly1305 encrypted communication with session key derivation
- **Platform Agnostic**: Clean abstraction layer allows porting to any platform
- **Modern C++20**: Uses `std::span`, concepts, and modern language features
- **Embedded-friendly**: No RTTI required; exception-free error handling via `common::Result` and status codes. (The library itself still compiles with exceptions enabled — required by nlohmann/json — but no library code throws.)

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
| [`examples/esp32-ip/`](examples/esp32-ip/) | IP | ESP-IDF | HAP over IP (WiFi, mDNS) |
| [`examples/linux/`](examples/linux/) | IP | Linux | HAP over IP using OpenSSL and Avahi |

## Project Structure

```
YAHAP/
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
│   ├── esp32-ip/          # ESP-IDF IP example
│   └── linux/             # Linux IP example
└── tests/                 # Unit tests
```

## Build Requirements

- **CMake** 3.20+
- **C++20** compatible compiler (GCC 10+, Clang 12+)
- **nlohmann/json** (fetched automatically via CMake for native builds; the
  ESP-IDF component manager provides it for ESP32 examples)

The ESP32 examples consume YAHAP directly as an ESP-IDF component (symlinked
at `examples/*/components/yahap`) and require the `yahap-pal` git submodules
(and, for the Linux example, the `DigitalDoorKey` submodule for HomeKey
support):

### Using as an ESP-IDF Component

The repository root is a valid ESP-IDF component. Add it to your project:

```bash
mkdir -p components && ln -s /path/to/YAHAP components/yahap
```

or place/copy the repository there. The component declares its sources and
its `nlohmann-json` dependency in the root `idf_component.yml`, which the
IDF component manager resolves automatically. Then add `yahap` to your
component's `REQUIRES`:

```cmake
idf_component_register(... REQUIRES yahap ...)
```

See `examples/esp32` and `examples/esp32-ip` for complete projects.

### Threading Contract

The library is intentionally single-threaded: call `start()`, `stop()`,
`tick()`, `add_accessory()` and all platform callbacks from one thread
(the PAL may use internal threads, but HAP-facing callbacks must be
marshalled back to that thread).

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

For the IP example, set WiFi credentials in `menuconfig` under
"HAP Example Configuration" (or `idf.py set-target` first):

### Building Linux Example

```bash
cd examples/linux
cmake -S . -B build
cmake --build build
./build/hap_lightbulb_example
```

## License

This library is licensed under the [MIT License](LICENSE).
