#pragma once

#include "hap/transport/HTTP.hpp"
#include "hap/transport/ConnectionContext.hpp"

namespace hap::transport {

/**
 * @brief Interface for HAP pairing endpoint handlers.
 *
 * Implemented by PairingEndpoints; BleTransport depends only on this
 * interface, which allows tests to inject lightweight mocks instead of
 * stubbing concrete class symbols (ODR-safe).
 */
class IPairingEndpoints {
public:
    virtual ~IPairingEndpoints() = default;

    virtual Response handle_pair_setup(const Request& req, ConnectionContext& ctx) = 0;
    virtual Response handle_pair_verify(const Request& req, ConnectionContext& ctx) = 0;
    virtual Response handle_pairings(const Request& req, ConnectionContext& ctx) = 0;

    /**
     * @brief Upgrade the connection to encrypted after a successful Pair Verify
     * M4 response has been sent. Default no-op for mocks.
     */
    virtual void complete_pair_verify(ConnectionContext&) {}
};

} // namespace hap::transport
