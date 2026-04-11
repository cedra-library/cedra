#pragma once

#include <cdr/types/types.h>
#include <cdr/calendar/date.h>
#include <cdr/calendar/freq.h>

#include <utility>

namespace cdr {

using FXPair = std::pair<std::string, std::string>;

class ForwardContract {
public:
    ForwardContract() = delete;

    ForwardContract(FXPair pair, DateType trade_date, Tenor tenor, f64 price = 0.)
        : pair_(std::move(pair))
        , trade_date_(std::move(trade_date))
        , tenor_(tenor)
        , price_(price)
    {}

    const FXPair& GetPair() const noexcept {
        return pair_;
    }

    [[nodiscard]] f64 GetPrice() const noexcept {
        return price_;
    }

    [[nodiscard]] const DateType& GetTradeDate() const noexcept {
        return trade_date_;
    }

    [[nodiscard]] const Tenor& GetTenor() const noexcept {
        return tenor_;
    }

    ForwardContract& SetPrice(f64 price) {
        price_ = price;
        return *this;
    }

private:
    FXPair pair_;
    DateType trade_date_;
    Tenor tenor_;
    f64 price_;
};


}  // namespace cdr
