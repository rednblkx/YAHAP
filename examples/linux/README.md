# Linux HAP Example

This directory contains a Linux example demonstrating the HAP library with platform-specific implementations.

## Building

```bash
cmake -S . -B build
cmake --build build
./build/hap_lightbulb_example
```

## Dependencies

- **OpenSSL 3.x**: Cryptographic operations
- **Avahi** (future): mDNS service discovery
- **C++20 compiler**: GCC 10+ or Clang 12+

## Architecture

The example demonstrates the HAP library's platform abstraction:

```
┌─────────────────────────────────────┐
│      Application (main.cpp)          │
│  Creates accessories & services      │
└──────────────┬──────────────────────┘
               │
┌──────────────▼──────────────────────┐
│      HAP Library (hap)               │
│  Protocol implementation (generic)   │
└──────────────┬──────────────────────┘
               │
┌──────────────▼──────────────────────┐
│   Linux PAL Implementation           │
│  - LinuxCrypto (OpenSSL)             │
│  - LinuxNetwork (Sockets + Avahi)    │
│  - LinuxStorage (JSON file)          │
│  - LinuxSystem (POSIX)               │
└──────────────────────────────────────┘
```

## License

Same as the parent HAP library project.
