#include <cdr/calendar/holiday_storage.h>

#include <chrono>
#include <stdexcept>

namespace cdr {

const std::set<DateType>& HolidayStorage::JurisdictionHolidays(const std::string& jur) const {
    auto it = storage.find(jur);
    if (it == storage.end()) {
        throw std::runtime_error("unknown jurisdiction");
    }

    return it->second;
}

bool HolidayStorage::IsWeekend(const std::string& jur, const DateType& date) const {
    if (JurisdictionHolidays(jur).contains(date)) {
        return true;
    }

    WeekDayType wd = Weekday(date);
    return wd == std::chrono::Saturday || wd == std::chrono::Sunday;
}

DateType HolidayStorage::FindNextWorkingDay(const std::string& jur, const DateType& date) const {
    DateType result = date;

    do {
        result = NextDay(result);
    } while (IsWeekend(jur, result));

    return result;
}

DateType HolidayStorage::FindPreviousWorkingDay(const std::string& jur, const DateType& date) const {
    DateType result = date;

    do {
        result = NextDay(result);
    } while (IsWeekend(jur, result));

    return result;
}

static i64 CountWeekends(const DateType& left, const DateType& right) {
    auto sys_left = std::chrono::sys_days(left);
    auto sys_right = std::chrono::sys_days(right);
    if (sys_right <= sys_left) [[unlikely]] {
        return 0;
    }
    auto diff = sys_right - sys_left;
    auto total_days = diff.count();
    i64 weekends = (total_days / 7) * 2;
    i32 remaining_days = total_days % 7;
    std::chrono::weekday wd_start = sys_left;
    for (i32 i = 0; i < remaining_days; i++) {
        if (wd_start == std::chrono::Saturday || wd_start == std::chrono::Sunday) {
            weekends++;
        }
        wd_start++;
    }
    return weekends;
}

[[nodiscard]] i64 HolidayStorage::CountBuisnessDays(const DateType& left, const DateType& right, const std::string& jur) const {
    auto num_weekends = CountWeekends(left, right);
    auto sys_left = std::chrono::sys_days(left).time_since_epoch().count();
    auto sys_right = std::chrono::sys_days(right).time_since_epoch().count();
    if (sys_left >= sys_right) [[unlikely]] {
        return 0;
    }
    auto it = storage.find(jur);
    if (it == storage.end()) [[unlikely]] {
        return (sys_right - sys_left) - num_weekends;
    }
    const auto& calendar = it->second;
    auto left_it = calendar.lower_bound(left);
    auto right_it = calendar.upper_bound(right);
    auto num_holidays = std::distance(left_it, right_it);
    return (sys_right - sys_left) - (num_weekends + num_holidays);
}

DateType HolidayStorage::AdjustWorkDay(const std::string& jur, DateType date, DateRollingRule rule) const {
    if (!IsWeekend(jur, date)) {
        return date;
    }

    switch (rule) {
    case DateRollingRule::kFollowing:
        return FindNextWorkingDay(jur, date);
    case DateRollingRule::kPreceding:
        return FindPreviousWorkingDay(jur, date);
    case DateRollingRule::kModifiedFollowing: {
        DateType adjusted = FindNextWorkingDay(jur, date);
        if (adjusted.month() != date.month()) {
            return FindPreviousWorkingDay(jur, date);
        }
        return adjusted;
    }
    case DateRollingRule::kUnadjusted:
        return date;
    }

    return date;
}

DateType HolidayStorage::AdvanceDateByBusinessDays(const std::string& jur, DateType date, i32 days) const {
    if (days >= 0) [[likely]] {
        for (i32 i = 0; i < days; i++) {
            date = FindNextWorkingDay(jur, date);
        }
    } else {
        for (i32 i = 0; i > days; i--) {
            date = FindPreviousWorkingDay(jur, date);
        }
    }
    return date;
}

DateType HolidayStorage::AdvanceDateByTenor(DateType date, Tenor tenor) const {
    switch (tenor.unit) {
        case TimeUnit::Day:
            return std::chrono::sys_days{date} + std::chrono::days{tenor.number};
        case TimeUnit::Week:
            return std::chrono::sys_days{date} + std::chrono::days{tenor.number * 7};
        case TimeUnit::Month:
            AddMonths(date, tenor.number);
            return date;
        case TimeUnit::Year:
            AddMonths(date, tenor.number * 12);
            return date;
        default:
            return std::chrono::year_month_day{};
    }
}

DateType HolidayStorage::AdvanceDateByConvention(const std::string& jur, DateType date, Tenor tenor, DateRollingRule rule) const {
    date = AdvanceDateByTenor(date, tenor);
    date = AdjustWorkDay(jur, date, rule);
    return date;
}

Generator<DateType> HolidayStorage::BusinessDays(Generator<DateType> dates, const std::string& jur,
                                                 DateRollingRule adjustment) const {
    DateType prev;
    for (DateType date : dates) {
        DateType adjusted = AdjustWorkDay(jur, date, adjustment);
        if (adjusted == prev) {
            continue;
        }
        co_yield adjusted;
        prev = adjusted;
    }
}

}  // namespace cdr
