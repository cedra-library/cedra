#pragma once

#include <cdr/calendar/date.h>
#include <cdr/calendar/freq.h>
#include <cdr/types/jurisdiction.h>

#include <concepts>
#include <algorithm>
#include <iterator>
#include <set>
#include <unordered_map>
#include <cdr/calendar/internal/export.h>

namespace cdr {

class CDR_CALENDAR_EXPORT HolidayStorage {
private:
    using StorageType = std::unordered_map<JurisdictionType, std::set<DateType>>;

public:
    HolidayStorage() = default;

    HolidayStorage(const HolidayStorage&) = delete;
    HolidayStorage& operator=(const HolidayStorage&) = delete;

    HolidayStorage(HolidayStorage&&) = default;
    HolidayStorage& operator=(HolidayStorage&&) = default;

    ~HolidayStorage() = default;

    inline void Insert(const JurisdictionType& jur, const DateType& date) {
        storage[jur].emplace(date);
    }

    bool IsWeekend(const JurisdictionType& jur, const DateType& date) const;
    bool IsBusinessDay(const JurisdictionType& jur, const DateType& date) const {
        return !IsWeekend(jur, date);
    }

    template <std::input_iterator InputIt>
    inline bool IsWeekendEachJur(const DateType& date, InputIt jur_begin, InputIt jur_end) const {
        static_assert(std::is_convertible_v<std::iter_value_t<InputIt>, JurisdictionType>,
                      "Expected iterators to Jurisdictions");

        return std::all_of(jur_begin, jur_end, [this, &date](const JurisdictionType& jur) { return IsWeekend(jur, date); });
    }

    template <std::input_iterator InputIt>
    inline bool IsWorkdayEachJur(const DateType& date, InputIt jur_begin, InputIt jur_end) const {
        static_assert(std::is_convertible_v<std::iter_value_t<InputIt>, JurisdictionType>,
                      "Expected iterators to Jurisdictions");

        return std::none_of(jur_begin, jur_end, [this, &date](const JurisdictionType& jur) { return IsWeekend(jur, date); });
    }

    template <std::input_iterator InputIt>
    bool AreWorkdays(const JurisdictionType& jur, InputIt days_begin, InputIt days_end) const {
        static_assert(std::is_convertible_v<std::iter_value_t<InputIt>, DateType>, "Expected iterators to dates");

        return std::all_of(days_begin, days_end, [this, &jur](const DateType& date) { return IsWeekend(jur, date); });
    }

    template <std::input_iterator InputIt>
    inline bool AreWeekends(const JurisdictionType& jur, InputIt days_begin, InputIt days_end) const {
        static_assert(std::is_convertible_v<std::iter_value_t<InputIt>, DateType>, "Expected iterators to dates");

        return std::none_of(days_begin, days_end, [this, &jur](const DateType& date) { return IsWeekend(jur, date); });
    }

    template <std::bidirectional_iterator DateIter, std::input_iterator JurIter>
    inline bool AreWorkdaysEachJur(DateIter date_begin, DateIter date_end, JurIter jur_begin, JurIter jur_end) {
        static_assert(std::is_convertible_v<std::iter_value_t<DateIter>, DateType>, "Expected iterators to dates");
        static_assert(std::is_convertible_v<std::iter_value_t<JurIter>, JurisdictionType>,
                      "Expected iterators to Jurisdictions");

        return std::all_of(jur_begin, jur_end,
                           [&](const JurisdictionType& jur) { return AreWorkdays(jur, date_begin, date_end); });
    }

    [[nodiscard]] DateType FindNextWorkingDay(const JurisdictionType& jur, const DateType& date) const;

    [[nodiscard]] DateType FindPreviousWorkingDay(const JurisdictionType& jur, const DateType& date) const;

    [[nodiscard]] int64_t CountBuisnessDays(const DateType& left, const DateType& right, const JurisdictionType& jur) const;

    bool Empty() const noexcept {
        return storage.empty();
    }

    void Clear() {
        return storage.clear();
    }

    struct HolidayStorageDeclarativeInit {
        HolidayStorageDeclarativeInit& operator()(const JurisdictionType& jur, const DateType& date) {
            parent->Insert(jur, date);
            return *this;
        }

        HolidayStorage* parent;
    };

    HolidayStorageDeclarativeInit StaticInit() {
        return HolidayStorageDeclarativeInit{this};
    }

    Generator<DateType> BusinessDays(Generator<DateType> dates, JurisdictionType jur,
                                     DateRollingRule adjustment = DateRollingRule::kFollowing) const;

    DateType AdjustWorkDay(const JurisdictionType& jur, DateType date, DateRollingRule adj) const;

    DateType AdvanceDateByBusinessDays(const JurisdictionType& jur, DateType date, i32 days) const;

    DateType AdvanceDateByTenor(DateType date, Tenor tenor) const;

    DateType AdvanceDateByConvention(const JurisdictionType& jur, DateType date, Tenor tenor, DateRollingRule rule = DateRollingRule::kFollowing) const;

private:
    const std::set<DateType>& JurisdictionHolidays(const JurisdictionType& jur) const;

private:
    StorageType storage;
};

}  // namespace cdr
