#pragma once
#include <cdr/types/expect.h>

namespace cdr {

enum class Error {
    ConractWithoutNPV = 0,
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
    return Failure<Error>(Error::ConractWithoutNPV);
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

} // namespace cdr