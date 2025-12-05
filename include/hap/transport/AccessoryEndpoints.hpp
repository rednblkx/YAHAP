#pragma once

#include "hap/transport/HTTP.hpp"
#include "hap/transport/ConnectionContext.hpp"
#include "hap/core/AttributeDatabase.hpp"
#include <nlohmann/json.hpp>

namespace hap::transport {

/**
 * @brief Accessory endpoint handlers
 */
class AccessoryEndpoints {
public:
    AccessoryEndpoints(core::AttributeDatabase* database);

    /**
     * @brief GET /accessories handler
     * Returns the complete attribute database in JSON format.
     */
    Response handle_get_accessories(const Request& req, ConnectionContext& ctx);

    /**
     * @brief GET /characteristics?id=<aid>.<iid>,... handler
     * Reads characteristic values.
     */
    Response handle_get_characteristics(const Request& req, ConnectionContext& ctx);

    /**
     * @brief PUT /characteristics handler
     * Writes characteristic values.
     */
    Response handle_put_characteristics(const Request& req, ConnectionContext& ctx);

    /**
     * @brief POST /prepare handler
     * Prepares a timed write transaction.
     */
    Response handle_prepare(const Request& req, ConnectionContext& ctx);

private:
    core::AttributeDatabase* database_;
    
    // Parse query string "id=1.2,1.3" into list of (aid, iid) pairs
    std::vector<std::pair<uint64_t, uint64_t>> parse_characteristic_ids(const std::string& query);
};

} // namespace hap::transport
