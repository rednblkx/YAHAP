#pragma once

#include "hap/transport/HTTP.hpp"
#include <functional>
#include <vector>
#include <string>
#include <optional>

namespace hap::transport {

// Forward declaration
class ConnectionContext;

/**
 * @brief HTTP Route Handler
 */
using RouteHandler = std::function<Response(const Request&, ConnectionContext&)>;

/**
 * @brief HTTP Route Definition
 */
struct Route {
    Method method;
    std::string path;
    RouteHandler handler;
    bool requires_pairing;  // True if endpoint requires verified pairing
};

/**
 * @brief Simple HTTP Router
 */
class Router {
public:
    Router();

    /**
     * @brief Register a route.
     */
    void add_route(Method method, std::string path, RouteHandler handler, bool requires_pairing = false);

    /**
     * @brief Dispatch an HTTP request to the appropriate handler.
     * @return Response, or nullopt if no route matches
     */
    std::optional<Response> dispatch(const Request& req, ConnectionContext& ctx);

private:
    std::vector<Route> routes_;
    
    bool path_matches(const std::string& pattern, const std::string& path);
};

} // namespace hap::transport
