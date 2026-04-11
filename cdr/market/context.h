#pragma once

#include <cdr/base/check.h>
#include <cdr/calendar/date.h>
#include <cdr/fx/fx.h>

#include <unordered_map>

namespace cdr {

class MarketContext {
public:
    [[nodiscard]] DateType Today() const noexcept {
        return today_;
    }

    [[nodiscard]] DateType SpotDate(const FXPair& pair) const noexcept;

    void SetToday(DateType date) noexcept {
        today_ = date;
    }

    f64 FxSpot(FXPair pair) const;
    void SetFxSpot(FXPair pair, f64 spot);

    HolidayStorage& Calendar();
    const HolidayStorage& Calendar() const;

private:
    std::unordered_map<FXPair, f64> fx_spots_;
    HolidayStorage calendar_;
    DateType today_;
};


}  // namespace cdr
