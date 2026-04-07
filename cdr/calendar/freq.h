#pragma once

namespace cdr {

enum class Freq {
    kAnnualy,      // Once a year
    kSemiAnnualy,  // Twice a year
    kQuarterly,    // Three times a year
    kMonthly,      // Every month
    kDaily,        // Every day
};

enum class TimeUnit {
    Day,
    Week,
    Month,
    Year,
};

struct Tenor {
    int number;
    TimeUnit unit;
};

enum class DateRollingRule {
    kFollowing,                     // Следующий рабочий день
    kPreceding,                     // Предыдущий рабочий день
    kModifiedFollowing,             // Модифицированный следующий рабочий день
    kUnadjusted,                    // Без подстройки (используется для генератора)
};

}  // namespace cdr
