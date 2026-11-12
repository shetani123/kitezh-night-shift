#include "kitezh/vessel.hpp"

#include <stdexcept>

namespace kitezh {

std::optional<std::string> validateVessel(const Vessel& vessel) {
    if (vessel.call_sign.empty()) {
        return "empty call sign";
    }
    for (char s : vessel.call_sign) {
        if (s == '\n' || s == ' ' || s == '\t' || s == '\r') {
            return "bad call sign";
        }
    }
    if (vessel.length_m <= 0 || vessel.length_m > 36) {
        return "bad length";
    }
    if (vessel.draft_dm <= 0) {
        return "bad draft";
    }
    return std::nullopt;
}

bool fitsInChamber(const std::vector<Vessel>& inside, const Vessel& next) {
    int sum_m = next.length_m;
    for (const auto& v : inside) {
        sum_m += v.length_m + kGapM;
    }
    if (sum_m <= kChamberLengthM) {
        return true;
    } else {
        return false;
    }

}

}  // namespace kitezh
