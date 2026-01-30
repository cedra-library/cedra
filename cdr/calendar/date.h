#pragma once

#include <chrono>
#include <type_traits>

#include <cdr/base/generator.h>
#include <cdr/calendar/freq.h>
#include <cdr/types/integers.h>
#include <cdr/types/floats.h>

using DateType = std::chrono::year_month_day;
using WeekDayType = std::chrono::weekday;

using SysDays = std::chrono::sys_days;

namespace cdr {

DateType NextDay(const DateType& date);

DateType PreviousDay(const DateType& ymd);

WeekDayType Weekday(const DateType& ymd);

int DayDifference(const DateType& lhs, const DateType& rhs);

unsigned LastMonthDay(const DateType& date);

bool IsLastMonthDay(const DateType& date);

// last_day -> last_day
void AddMonths(DateType& ymd, i32 months);

inline constexpr u64 DaysInYear(const DateType& date) {
    return date.year().is_leap() ? 366 : 365;
}

inline constexpr u32 DaysTillTheEndOfYear(const DateType& date) {
    if (date.year().is_leap()) {
        return 366 - static_cast<u32>(date.day());
    }
    return 365 - static_cast<u32>(date.day());
}

DateType NextYearBeginning(const DateType& date);

constexpr struct EternityBeforeType {} EternityBefore;
constexpr struct EternityAfterType {} EternityAfter;

struct Period {
    using DiffType = i32;

    DateType since;
    DateType until;

    [[nodiscard]] bool Valid() const {
        return since.ok() && until.ok() && since <= until;
    }

    [[nodiscard]] Generator<DateType> WithFrequency(Freq freq) const;

    [[nodiscard]] const DateType& Since() const noexcept {
        return since;
    }
    [[nodiscard]] const DateType& Until() const noexcept {
        return until;
    }

    [[nodiscard]] DiffType Days() const noexcept {
        return DayDifference(Until(), Since());
    }

    // DC factors:
    [[nodiscard]] f64 Act360() const noexcept {
        return static_cast<f64>(Days()) / 360;
    }

    [[nodiscard]] f64 Act365() const noexcept {
        return static_cast<f64>(Days()) / 365;
    }

    [[nodiscard]] f64 ActActISDA() const;

    [[nodiscard]] bool Contains(const Period& other) const noexcept {
        return Since() <= other.Since() && Until() >= other.Until();
    }

    [[nodiscard]] bool SameYear() const noexcept {
        return Since().year() == Until().year();
    }
};

template <std::size_t I>
DateType& get(Period& s);

template <std::size_t I>
const DateType& get(const Period& s);

enum class DcConvention {
    kAct360,
    kAct365,
    kActActISDA,
};

[[nodiscard]] f64 DayCountFraction(const Period& period, DcConvention method = DcConvention::kActActISDA);

}  // namespace cdr
