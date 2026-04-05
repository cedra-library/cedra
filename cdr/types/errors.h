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
    __NumberOfErrors,
};

inline Failure<Error> ErrorContractWithoutNPV() {
    return Failure<Error>(Error::ContractWithoutNPV);
}

inline Failure<Error> ErrorContractWithoutUnderlyingAsset() {
    return Failure<Error>(Error::ContractWithoutUnderlyingAsset);
}

inline Failure<Error> ErrorContractWithoutExpirationDate() {
    return Failure<Error>(Error::ContractWithoutExpirationDate);
}

inline Failure<Error> ErrorContractWithoutType() {
    return Failure<Error>(Error::ContractWithoutType);
}

inline Failure<Error> ErrorContractWithoutStyle() {
    return Failure<Error>(Error::ContractWithoutStyle);
}

inline Failure<Error> ErrorContractWithoutStrike() {
    return Failure<Error>(Error::ContractWithoutStrike);
}

inline Failure<Error> ErrorNegativeStrike() {
    return Failure<Error>(Error::NegativeStrike);
}

inline Failure<Error> ErrorDateInAPast() {
    return Failure<Error>(Error::DateInAPast);
}

inline Failure<Error> ErrorNoMemory() {
    return Failure<Error>(Error::NoMemory);
}

inline Failure<Error> ErrorNoData() {
    return Failure<Error>(Error::NoData);
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