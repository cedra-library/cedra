#pragma once

#include <chrono>
#include <type_traits>

#include <cdr/base/generator.h>
#include <cdr/calendar/freq.h>
#include <cdr/types/integers.h>
#include <cdr/types/floats.h>

#include <cdr/calendar/internal/export.h>
using DateType = std::chrono::year_month_day;
using WeekDayType = std::chrono::weekday;

using SysDays = std::chrono::sys_days;
using DayDiffType = int;

namespace cdr {

CDR_CALENDAR_EXPORT DateType Today();

CDR_CALENDAR_EXPORT DateType NextDay(const DateType& date);

CDR_CALENDAR_EXPORT DateType PreviousDay(const DateType& ymd);

CDR_CALENDAR_EXPORT WeekDayType Weekday(const DateType& ymd);

CDR_CALENDAR_EXPORT DayDiffType DayDifference(const DateType& lhs, const DateType& rhs);

CDR_CALENDAR_EXPORT unsigned LastMonthDay(const DateType& date);

CDR_CALENDAR_EXPORT bool IsLastMonthDay(const DateType& date);

CDR_CALENDAR_EXPORT void AddMonths(DateType& ymd, i32 months);

CDR_CALENDAR_EXPORT DateType AddDays(const DateType& ymd, unsigned days);

inline constexpr u64 DaysInYear(const DateType& date) {
    return date.year().is_leap() ? 366 : 365;
}

inline constexpr u32 DaysTillTheEndOfYear(const DateType& date) {
    if (date.year().is_leap()) {
        return 366 - static_cast<u32>(date.day());
    }
    return 365 - static_cast<u32>(date.day());
}

CDR_CALENDAR_EXPORT DateType NextYearBeginning(const DateType& date);

constexpr struct EternityBeforeType {} EternityBefore;
constexpr struct EternityAfterType {} EternityAfter;

struct CDR_CALENDAR_EXPORT Period {
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

    [[nodiscard]] DayDiffType Days() const noexcept {
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

CDR_CALENDAR_EXPORT f64 DayCountFraction(const Period& period, DcConvention method = DcConvention::kActActISDA);

}  // namespace cdr
