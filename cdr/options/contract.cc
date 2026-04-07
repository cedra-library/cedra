#include <cdr/options/contract.h>

namespace cdr {

Expect<OptionContract, Error> OptionContractBuilder::Build() noexcept {
    if (!underlying_asset_) [[unlikely]] {
        return ErrorContractWithoutUnderlyingAsset();
    }

    if (!expiration_date_) [[unlikely]] {
        return ErrorContractWithoutExpirationDate();
    }

    if (!type_) [[unlikely]] {
        return ErrorContractWithoutType();
    }

    if (!style_) [[unlikely]] {
        return ErrorContractWithoutStyle();
    }

    if (!strike_price_) [[unlikely]] {
        return ErrorContractWithoutStrike();
    }

    if (strike_price_.value() < 0) [[unlikely]] {
        return ErrorNegativeStrike();
    }

    OptionContract contract;

    contract.underlying_asset_ = std::move(underlying_asset_).value();
    contract.expiration_date_ = std::move(expiration_date_).value();
    contract.type_ = std::move(type_).value();
    contract.style_ = std::move(style_).value();
    contract.strike_price_ = std::move(strike_price_).value();

    Reset();

    return Ok(std::move(contract));
}

void OptionContractBuilder::Reset() {
    underlying_asset_.reset();
    expiration_date_.reset();
    type_.reset();
    style_.reset();
    strike_price_.reset();
}

}  // namespace cdr