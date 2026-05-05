#pragma once

#include <cdr/types/types.h>
#include <cdr/base/check.h>

#include <algorithm>
#include <bit>
#include <string_view>
#include <ostream>
#include <charconv>
#include <cstring>
#include <iostream>

namespace stdr = std::ranges;

class CurrencyTag {
public:
    // implicit
    constexpr CurrencyTag(const char* str) noexcept: data_{} {
        size_t str_size = std::char_traits<char>::length(str);

        // NOTE: CDR_CHECK is not constexpr
        if (str_size >= 8) [[unlikely]] {
            std::cerr << "Fatal: currency tag too long: " << str << std::endl;
            std::terminate();
        }
        stdr::copy_n(str, str_size, data_);
    }

    constexpr std::string_view Str() const noexcept {
        return data_;
    }

    constexpr bool operator==(CurrencyTag other) const noexcept {
        return stdr::equal(data_, other.data_);
    }

    constexpr bool operator<(CurrencyTag other) const noexcept {
        return std::bit_cast<u64>(*this) < std::bit_cast<u64>(other);
    }

private:
    alignas(8) char data_[8];
};

inline std::ostream& operator<<(std::ostream& os, CurrencyTag tag) {
    return os << tag.Str();
}

namespace std {

template <>
struct hash<CurrencyTag> {
    hash<u64> hashfunc_;
    u64 operator()(CurrencyTag tag) const noexcept {
        return hashfunc_(std::bit_cast<u64>(tag));
    }
};

}  // namespace std


static_assert(sizeof(std::hash<CurrencyTag>) == 1);
static_assert(sizeof(CurrencyTag) == 8);
static_assert(std::is_trivially_copyable_v<CurrencyTag>);

using JurisdictionType = CurrencyTag;
