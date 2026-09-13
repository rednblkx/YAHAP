# YAHAP (Yet Another HAP)

A modern C++20 implementation of the HomeKit Accessory Protocol (HAP) with a focus on modularity and platform independence. Supports both **HAP over IP** and **HAP over BLE** transports.

## Features

- **Dual Transport Support**: Full implementation of both HAP over IP (TCP/HTTP) and HAP over BLE
- **Complete Pairing Protocol**: SRP-6a based Pair-Setup and Pair-Verify
- **Secure Sessions**: ChaCha20-Poly1305 encrypted communication with session key derivation
- **Platform Agnostic**: Clean abstraction layer allows porting to any platform
- **Modern C++20**: Uses `std::span`, concepts, and modern language features
- **Embedded-friendly**: No RTTI, no exceptions, no external dependencies. Error handling via `common::Result` and status codes; the library is always compiled with `-fno-exceptions -fno-rtti -fno-unwind-tables -fno-threadsafe-statics`.

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
| `AttributeDatabase` | HAP object model (Accessories, Services, Characteristics) with JSON serialization. Unique ownership (`std::unique_ptr`) throughout; lookups return raw non-owning pointers |
| `ServiceBuilder` | Generic table-driven service builder; the full HAP characteristic catalog is a compact ROM descriptor table (`CharacteristicTypes.hpp`) |
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
│   ├── AccessoryServer.hpp
│   ├── common/            # Result, TaskScheduler
│   ├── core/              # HAP object model
│   │   ├── Accessory.hpp
│   │   ├── Service.hpp
│   │   ├── Characteristic.hpp
│   │   ├── AttributeDatabase.hpp
│   │   ├── IIDManager.hpp
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
│   │   ├── Router.hpp
│   │   ├── SecureSession.hpp
│   │   ├── BleTransport.hpp
│   │   ├── ble/           # HAP-BLE PDU, sessions, TLV builder
│   │   └── PairingEndpoints.hpp
│   ├── pairing/           # Pairing protocol
│   │   ├── PairSetup.hpp
│   │   └── PairVerify.hpp
│   └── types/             # Characteristic / Service type definitions
├── src/                   # Implementation files (mirrors include/hap/)
├── examples/
│   ├── esp32/             # ESP-IDF BLE example
│   ├── esp32-ip/          # ESP-IDF IP example
│   └── linux/             # Linux IP example
└── tests/                 # Unit tests
```

## Build Requirements

- **CMake** 3.20+
- **C++20** compatible compiler (GCC 10+, Clang 12+)

The library has no external dependencies: it ships its own minimal JSON value
type (`hap/common/JsonValue.hpp`) sized for the HAP protocol surface, and all
logging is compile-time gated by `HAP_LOG_LEVEL` (0=Debug, 1=Info [default],
2=Warning, 3=Error — set via `-DYAHAP_LOG_LEVEL=<n>` when building the
library; lower levels drop log strings and formatting code from the binary
entirely).

The library compiles unconditionally with an embedded flag set (applied as
PUBLIC compile options in both CMake and ESP-IDF component modes, so consumers
build with a matching ABI):

- `-fno-exceptions -fno-rtti` — nothing in the library throws or uses RTTI
- `-fno-unwind-tables -fno-asynchronous-unwind-tables` — no exception unwinding data
- `-ffunction-sections -fdata-sections` — lets the linker strip unused
  services and endpoints from the final image (`--gc-sections` / `-dead_strip`)
- `-fno-threadsafe-statics` — all function-local statics are
  constant-initialized and the HAP threading contract is single-threaded

The ESP32 examples consume YAHAP directly as an ESP-IDF component (symlinked
at `examples/*/components/yahap`) and require the `yahap-pal` git submodules
(and, for the Linux example, the `DigitalDoorKey` submodule for HomeKey
support):

### Using as an ESP-IDF Component

The repository root is a valid ESP-IDF component. Add it to your project:

```bash
mkdir -p components && ln -s /path/to/YAHAP components/yahap
```

Requires ESP-IDF v6.x (the examples are developed against v6.1).

or place/copy the repository there. The component declares its sources in the
root `CMakeLists.txt` and has no dependencies to resolve. Then add `yahap` to
your component's `REQUIRES`:

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
