#pragma once

#include <cdr/base/check.h>
#include <cdr/calendar/date.h>
#include <cdr/market/context.h>

#include <memory>

namespace cdr {

class Model {
public:
    [[nodiscard]] MarketContext& Context() noexcept;
    CurveSet& Curves() noexcept;

    void SetToday(DateType d);
    void SetFxSpot(FXPair pair, f64 spot);

    void Rebuild(NodeId id);

    // void RebuildAffected();

private:
    MarketContext context_;
    CurveSet curves_;
    DependencyGraph curve_deps_;

};


}  // namespace cdr
