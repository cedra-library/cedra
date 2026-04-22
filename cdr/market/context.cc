#include <cdr/market/context.h>

namespace cdr {

DateType MarketContext::SpotDate(const FXPair& pair) const noexcept {
    // TODO: Spot settlement rule
    return Today();
}

f64 MarketContext::FxSpot(const FXPair& pair) const {
    auto iter = fx_spots_.find(pair);
    if (iter != fx_spots_.end()) [[likely]] {
        return iter->second;
    }
    iter = fx_spots_.find(pair.Reversed());
    CDR_CHECK(iter != fx_spots_.end());
    return 1. / iter->second;
}

void MarketContext::SetFxSpot(const FXPair& pair, f64 spot) {
    fx_spots_.insert_or_assign(pair, spot);
}

}  // namespace cdr
