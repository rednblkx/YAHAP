#include "hap/transport/Router.hpp"
#include "hap/transport/ConnectionContext.hpp"

#include <string_view>

namespace hap::transport {

Router::Router() = default;

void Router::add_route(Method method, std::string path, RouteHandler handler, bool requires_pairing) {
    routes_.push_back({method, std::move(path), std::move(handler), requires_pairing});
}

std::optional<Response> Router::dispatch(const Request& req, ConnectionContext& ctx) {
    for (const auto& route : routes_) {
        if (route.method == req.method && path_matches(route.path, req.path)) {
            // Check pairing requirement
            if (route.requires_pairing && !ctx.is_encrypted()) {
                Response resp{Status::BadRequest};
                resp.set_body("Pairing required");
                return resp;
            }
            
            // Call handler
            return route.handler(req, ctx);
        }
    }
    
    // No route found
    return std::nullopt;
}

bool Router::path_matches(const std::string& pattern, const std::string& path) {
    size_t query_start = path.find('?');
    std::string_view path_base = (query_start == std::string::npos) 
        ? std::string_view(path) 
        : std::string_view(path).substr(0, query_start);
        
    return pattern == path_base;
}

} // namespace hap::transport
