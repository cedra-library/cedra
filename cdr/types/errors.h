#pragma once
#include <cdr/types/expect.h>

namespace cdr {
enum class Error {
    ContractWithoutNPV = 0,
    ContractWithoutUnderlyingAsset,
    ContractWithoutExpirationDate,
    ContractWithoutType,
    ContractWithoutStyle,
    ContractWithoutStrike,
    NegativeStrike,
    DateInAPast,
    NoMemory,
    NoData,
    ExtrapolationNotAllowed,
    TimeExtrapolationNotAllowed,
    StrikeExtrapolationNotAllowed,
    __NumberOfErrors,
};

constexpr Failure<Error> ErrorContractWithoutNPV() {
    return Failure<Error>(Error::ContractWithoutNPV);
}

constexpr Failure<Error> ErrorContractWithoutUnderlyingAsset() {
    return Failure<Error>(Error::ContractWithoutUnderlyingAsset);
}

constexpr Failure<Error> ErrorContractWithoutExpirationDate() {
    return Failure<Error>(Error::ContractWithoutExpirationDate);
}

constexpr Failure<Error> ErrorContractWithoutType() {
    return Failure<Error>(Error::ContractWithoutType);
}

constexpr Failure<Error> ErrorContractWithoutStyle() {
    return Failure<Error>(Error::ContractWithoutStyle);
}

constexpr Failure<Error> ErrorContractWithoutStrike() {
    return Failure<Error>(Error::ContractWithoutStrike);
}

constexpr Failure<Error> ErrorNegativeStrike() {
    return Failure<Error>(Error::NegativeStrike);
}

constexpr Failure<Error> ErrorDateInAPast() {
    return Failure<Error>(Error::DateInAPast);
}

constexpr Failure<Error> ErrorNoMemory() {
    return Failure<Error>(Error::NoMemory);
}

constexpr Failure<Error> ErrorNoData() {
    return Failure<Error>(Error::NoData);
}

constexpr Failure<Error> ErrorExtrapolationNotAllowed() {
    return Failure<Error>(Error::ExtrapolationNotAllowed);
}

constexpr Failure<Error> ErrorTimeExtrapolationNotAllowed() {
    return Failure<Error>(Error::TimeExtrapolationNotAllowed);
}

constexpr Failure<Error> ErrorStrikeExtrapolationNotAllowed() {
    return Failure<Error>(Error::StrikeExtrapolationNotAllowed);
}

[[nodiscard]] constexpr std::string_view ErrorAsStringView(const Error error) noexcept {
    switch (error) {
    case Error::ContractWithoutNPV:               return "Contract without NPV";
    case Error::ContractWithoutUnderlyingAsset:  return "Contract without underlying asset";
    case Error::ContractWithoutExpirationDate:   return "Contract without expiration date";
    case Error::ContractWithoutType:             return "Contract without type";
    case Error::ContractWithoutStyle:            return "Contract without style";
    case Error::ContractWithoutStrike:           return "Contract without strike";
    case Error::NegativeStrike:                  return "Negative strike price";
    case Error::DateInAPast:                     return "Date is in the past";
    case Error::NoMemory:                        return "Out of memory";
    case Error::NoData:                          return "No data available";
    case Error::ExtrapolationNotAllowed:         return "Extrapolation is not allowed";
    case Error::TimeExtrapolationNotAllowed:     return "Time extrapolation is not allowed";
    case Error::StrikeExtrapolationNotAllowed:   return "Strike extrapolation is not allowed";
    case Error::__NumberOfErrors:                [[fallthrough]];
    default:                                     return "Unknown error";
    }
}

[[nodiscard]] constexpr std::string_view ErrorAsStringView(Failure<Error> failure) noexcept {
    return ErrorAsStringView(failure.Value());
}

template<typename T>
[[nodiscard]] constexpr std::string_view ErrorAsStringView(const Expect<T, Error>& failure) noexcept {
    return ErrorAsStringView(failure.GetFailure());
}

} // namespace cdr
