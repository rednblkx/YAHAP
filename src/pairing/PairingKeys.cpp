#include "hap/pairing/PairingKeys.hpp"
#include <algorithm>

namespace hap::pairing {

bool load_accessory_ltk(platform::Storage* storage,
                        std::array<uint8_t, 64>& ltsk,
                        std::array<uint8_t, 32>& ltpk) {
    if (!storage) return false;
    auto ltsk_data = storage->get(kAccessoryLTSKKey);
    auto ltpk_data = storage->get(kAccessoryLTPKKey);
    if (!ltsk_data || !ltpk_data || ltsk_data->size() != 64 || ltpk_data->size() != 32) {
        return false;
    }
    std::copy_n(ltsk_data->begin(), 64, ltsk.begin());
    std::copy_n(ltpk_data->begin(), 32, ltpk.begin());
    return true;
}

} // namespace hap::pairing
