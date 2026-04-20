#pragma once

#include <cdr/calendar/date.h>
#include <cdr/types/options.h>
#include <cdr/types/errors.h>
#include <cdr/types/expect.h>
#include <cdr/options/internal/export.h>

#include <optional>
#include <string>

namespace cdr {
class OptionContractBuilder;

class CDR_OPTIONS_EXPORT OptionContract final {
public:
    OptionContract(const OptionContract&) = delete;
    OptionContract& operator=(const OptionContract&) = delete;

    OptionContract(OptionContract&&) noexcept = default;
    OptionContract& operator=(OptionContract&&) noexcept = default;

    friend class OptionContractBuilder;

    [[nodiscard]] const std::string& UnderlyingAsset() const noexcept {
        return underlying_asset_;
    }

    [[nodiscard]] DateType ExpirationDate() const noexcept {
        return expiration_date_;
    }

    [[nodiscard]] OptionType Type() const noexcept {
        return type_;
    }

    [[nodiscard]] OptionStyle Style() const noexcept {
        return style_;
    }

    [[nodiscard]] f64 StrikePrice() const noexcept {
        return strike_price_;
    }

private:
    OptionContract() = default;

private:
    std::string underlying_asset_{};
    DateType expiration_date_{};
    OptionType type_{};
    OptionStyle style_{};
    f64 strike_price_{};
};

class CDR_OPTIONS_EXPORT OptionContractBuilder final {
public:
    OptionContractBuilder(OptionStyle style, OptionType type) : type_(type), style_(style) {
    }

    [[maybe_unused]] OptionContractBuilder& UnderlyingAsset(std::string asset) {
        underlying_asset_ = std::move(asset);
        return *this;
    }

    [[maybe_unused]] OptionContractBuilder& ExpirationDate(const DateType& date) {
        expiration_date_ = date;
        return *this;
    }
    [[maybe_unused]] OptionContractBuilder& Style(OptionStyle style) {
        style_ = style;
        return *this;
    }

    [[maybe_unused]] OptionContractBuilder& StrikePrice(f64 strike_price) {
        strike_price_ = strike_price;
        return *this;
    }

    [[maybe_unused]] OptionContractBuilder& ContractType(OptionType type) {
        type_ = type;
        return *this;
    }

    void Reset();

    [[nodiscard]] Expect<OptionContract, Error> Build() noexcept;

private:
    std::optional<std::string> underlying_asset_;
    std::optional<DateType> expiration_date_;
    std::optional<OptionType> type_;
    std::optional<OptionStyle> style_;
    std::optional<f64> strike_price_;
};

}  // namespace cdr