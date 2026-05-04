#pragma once

#include <cdr/market/internal/export.h>
#include <cdr/base/check.h>
#include <cdr/calendar/date.h>
#include <cdr/calendar/holiday_storage.h>
#include <cdr/fx/fx.h>

#include <unordered_map>
#include <functional>

namespace cdr {

class MarketContextView;

class CDR_MARKET_EXPORT MarketContext {
public:
    MarketContext(HolidayStorage&& calendar, DateType today)
        : fx_spots_()
        , calendar_(std::move(calendar))
        , today_(today)
    {}

    MarketContext(const MarketContext&) = delete;
    MarketContext& operator=(const MarketContext&) = delete;

    [[nodiscard]] DateType Today() const noexcept {
        return today_;
    }

    [[nodiscard]] DateType SpotDate(const FXPair& pair) const noexcept;

    void SetToday(DateType date) noexcept {
        today_ = date;
    }

    [[nodiscard]] f64 FxSpot(const FXPair& pair) const;
    void SetFxSpot(const FXPair& pair, f64 spot);

    [[nodiscard]] const HolidayStorage& Calendar() const {
        return calendar_;
    };

private:
    std::unordered_map<FXPair, f64> fx_spots_;
    HolidayStorage calendar_;
    DateType today_;
};

class CDR_MARKET_EXPORT MarketContextView {
public:
    MarketContextView(const MarketContext& context) : context_(context) {};

    [[nodiscard]] DateType Today() const noexcept {
        return context_.Today();
    }

    [[nodiscard]] DateType SpotDate(const FXPair& pair) const noexcept {
        return context_.SpotDate(pair);
    }

    [[nodiscard]] f64 FxSpot(const FXPair& pair) const {
        return context_.FxSpot(pair);
    }

    [[nodiscard]] const HolidayStorage& Calendar() const {
        return context_.Calendar();
    }
private:
    const MarketContext& context_;
};

}  // namespace cdr
