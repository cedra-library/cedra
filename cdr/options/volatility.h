#pragma once

#include <cdr/types/errors.h>
#include <cdr/types/expect.h>
#include <cdr/types/types.h>
#include <cdr/calendar/date.h>
#include <atomic>
#include <cdr/options/internal/export.h>
#include <cdr/base/aligned_alloc.h>
#include <cdr/base/hardware_interference_size.h>
#include <algorithm>
#include <memory>
#include <ranges>
#include <map>
#include <vector>
#include <cstring>
#include <cdr/options/interpolation/quadratic_spline.h>
#include <cdr/options/interpolation/lerp.h>
#include <cdr/options/interpolation/flat_forward.h>
#include <cdr/curve/interpolation/linear.h>
#include <cdr/curve/curve.h>
#include <cdr/options/helpers.h>


namespace cdr {


// Forward decl
template<typename Interpolation>
class VolatilitySurfaceProvider;

namespace internal {

// Used in VolatilitySurface::Volatility
template<typename Iter>
constexpr u64 IndexFromIterator(Iter begin, u64 size, Iter it) {
    u64 idx = (it == begin) ? 0 : std::distance(begin, it);
    if (size > 1 && idx >= (size-1)) {
        idx = size - 2;
    }

    return idx;
}

inline u64 AlignSize(const u64 size, const u64 alignment) noexcept {
    CDR_CHECK((alignment & (alignment - 1)) == 0) << "Alignment must be a power of two.";
    const u64 mask = alignment - 1;
    return (size + mask) & ~mask;
}

inline u64 AlignToCacheLine(const u64 size) noexcept {
    return AlignSize(size, kHardwareDestructiveInterferenceSize);
}

} // namespace internal

template<typename Interpolation = QuadraticSplineInterpolator>
class VolatilitySurface {
public:

    struct SurfaceHeader {
        DateType today;
        static_assert(sizeof(DateType) == sizeof(u32));

        u32 strikes_size;
        u32 dates_size;
        u32 pillar_deltas_size;

        u32 strikes_byte_offset;
        u32 dates_byte_offset;
        u32 pillar_deltas_byte_offset;
        u32 states_byte_offset;
        u32 deltas_byte_offset;

        f64 spot;

        mutable std::atomic<u32> reference_count;

        size_t total_size_in_bytes;
    };

    using StrikeVolatilityType = typename Interpolation::StrikeVolatilityType;
    using DeltaVolatilityType = typename Interpolation::DeltaVolatilityType;

    static constexpr f64 kTimeEpsilon = 1e-4;
    static constexpr f64 kStrikeEpsilon = 1e-4;
    static constexpr f64 kDeltaEpsilon = 1e-4;

public:

    explicit VolatilitySurface(void* incremented_base) {
        CDR_CHECK(incremented_base != nullptr) << "VolatilitySurface cannot be nullptr.";
        header_ptr_ = static_cast<SurfaceHeader*>(incremented_base); auto* base_ptr = static_cast<std::byte*>(incremented_base);
        strikes_ptr_ = reinterpret_cast<f64*>(base_ptr + header_ptr_->strikes_byte_offset);
        pillar_deltas_ptr_ = reinterpret_cast<f64*>(base_ptr + header_ptr_->pillar_deltas_byte_offset);
        dates_ptr_ = reinterpret_cast<f64*>(base_ptr + header_ptr_->dates_byte_offset);
        spline_coefficients_ptr_ = reinterpret_cast<StrikeVolatilityType*>(base_ptr + header_ptr_->states_byte_offset);
        deltas_volatility_ptr_ = reinterpret_cast<DeltaVolatilityType*>(base_ptr + header_ptr_->deltas_byte_offset);
    }

    VolatilitySurface(const VolatilitySurface& other) noexcept {
        other.Header().reference_count.fetch_add(1, std::memory_order_acq_rel);

        header_ptr_ = other.header_ptr_;
        strikes_ptr_ = other.strikes_ptr_;
        pillar_deltas_ptr_ = other.pillar_deltas_ptr_;
        dates_ptr_ = other.dates_ptr_;
        spline_coefficients_ptr_ = other.spline_coefficients_ptr_;
        deltas_volatility_ptr_ = other.deltas_volatility_ptr_;
    }

    VolatilitySurface& operator=(const VolatilitySurface& other) noexcept {
        if (&other != this) [[likely]] {
            if (other.header_ptr_) {
                other.Header().reference_count.fetch_add(1, std::memory_order_acq_rel);
            }
            this->Reclaim();

            header_ptr_ = other.header_ptr_;
            strikes_ptr_ = other.strikes_ptr_;
            pillar_deltas_ptr_ = other.pillar_deltas_ptr_;
            dates_ptr_ = other.dates_ptr_;
            spline_coefficients_ptr_ = other.spline_coefficients_ptr_;
            deltas_volatility_ptr_ = other.deltas_volatility_ptr_;
        }

        return *this;
    }

    VolatilitySurface(VolatilitySurface&& other) noexcept
        : header_ptr_(std::exchange(other.header_ptr_, nullptr))
        , strikes_ptr_(std::exchange(other.strikes_ptr_, nullptr))
        , pillar_deltas_ptr_(std::exchange(other.pillar_deltas_ptr_, nullptr))
        , dates_ptr_(std::exchange(other.dates_ptr_, nullptr))
        , spline_coefficients_ptr_(std::exchange(other.spline_coefficients_ptr_, nullptr))
        , deltas_volatility_ptr_(std::exchange(other.deltas_volatility_ptr_, nullptr))
    {}


    VolatilitySurface& operator=(VolatilitySurface&& other) noexcept {
        if (&other != this) [[likely]] {
            this->Reclaim();

            header_ptr_ = std::exchange(other.header_ptr_, nullptr);
            strikes_ptr_ = std::exchange(other.strikes_ptr_, nullptr);
            pillar_deltas_ptr_ = std::exchange(other.pillar_deltas_ptr_, nullptr);
            dates_ptr_ = std::exchange(other.dates_ptr_, nullptr);
            spline_coefficients_ptr_ = std::exchange(other.spline_coefficients_ptr_, nullptr);
            deltas_volatility_ptr_ = std::exchange(other.deltas_volatility_ptr_, nullptr);
        }
        return *this;
    }

    virtual ~VolatilitySurface() {
        if (header_ptr_) {
            this->Reclaim();
        }
    }

    [[nodiscard]] const SurfaceHeader& Header() const {
        return *header_ptr_;
    }

    [[nodiscard]] std::span<const f64> Strikes() const noexcept {
        return {strikes_ptr_, header_ptr_->strikes_size};
    }

    [[nodiscard]] std::span<const f64> Dates() const noexcept {
        return {dates_ptr_, header_ptr_->dates_size};
    }

    [[nodiscard]] std::span<const f64> PillarDeltas() const noexcept {
        return {pillar_deltas_ptr_, header_ptr_->pillar_deltas_size};
    }

    [[nodiscard]] cdr::Expect<f64, Error> Volatility(const DateType& date, f64 strike) const noexcept {
        if (header_ptr_ == nullptr) [[unlikely]] {
            return ErrorNoData();
        }

        const auto strikes_span = Strikes();

        // Clamp to boundary if within epsilon to prevent spline extrapolation "dives"
        if (strike < strikes_span.front()) {
            if (strike < strikes_span.front() - kStrikeEpsilon) return ErrorStrikeExtrapolationNotAllowed();
            strike = strikes_span.front();
        } else if (strike > strikes_span.back()) {
            if (strike > strikes_span.back() + kStrikeEpsilon) return ErrorStrikeExtrapolationNotAllowed();
            strike = strikes_span.back();
        }

        const u32 strikes_size = header_ptr_->strikes_size;
        return InterpolateInTime(date, [&](u64 idx) {
            return Interpolation::Evaluate(&spline_coefficients_ptr_[idx * strikes_size], strikes_span, strike)
                .OrCrashProgram();
        });
    }

    [[nodiscard]] cdr::Expect<f64, Error> VolatilityByDelta(const DateType& date, f64 delta) const noexcept {
        if (header_ptr_ == nullptr) [[unlikely]] {
            return ErrorNoData();
        }

        const auto deltas_span = PillarDeltas();
        if (delta < deltas_span.front() - kDeltaEpsilon || delta > deltas_span.back() + kDeltaEpsilon) [[unlikely]] {
            return ErrorDeltaExtrapolationNotAllowed();
        }

        const u64 deltas_size = header_ptr_->pillar_deltas_size;
        return InterpolateInTime(date, [&](u64 idx) {
            return Interpolation::Evaluate(&deltas_volatility_ptr_[idx * deltas_size], deltas_span, delta).OrCrashProgram();
        });
    }

private:

    [[maybe_unused]] bool Reclaim() noexcept {
        if (!header_ptr_) {
            return false;
        }

        if (const u64 remaining = header_ptr_->reference_count.fetch_sub(1, std::memory_order_acq_rel); remaining == 1) {
            cdr::AlignedFree(const_cast<SurfaceHeader*>(header_ptr_));
            header_ptr_ = nullptr;
            return true;
        }

        header_ptr_ = nullptr;
        return false;
    }

    template<typename F>
    [[nodiscard]] cdr::Expect<f64, Error> InterpolateInTime(const DateType& date, F&& evaluate_at_idx) const noexcept {
        const f64 target_time = Period{Header().today, date}.Act365();
        const auto dates_span = Dates();

        // Проверка границ по времени
        if (target_time < dates_span.front() - kTimeEpsilon || target_time > dates_span.back() + kTimeEpsilon) {
            return ErrorTimeExtrapolationNotAllowed();
        }

        auto date_it = std::ranges::lower_bound(dates_span, target_time);
        u64 date_idx = internal::IndexFromIterator(dates_span.begin(), dates_span.size(), date_it);

        const f64 vol_t1 = evaluate_at_idx(date_idx);

        // Если попали точно в дату или в наборе всего одна дата
        if (dates_span[date_idx] == target_time || dates_span.size() == 1) {
            return Ok<f64>(vol_t1);
        }

        const f64 vol_t2 = evaluate_at_idx(date_idx + 1);
        return Ok(FlatForward(target_time, dates_span[date_idx], vol_t1, dates_span[date_idx + 1], vol_t2));
    }
private:
    const SurfaceHeader* header_ptr_;
    const f64* pillar_deltas_ptr_;
    const f64* strikes_ptr_;
    const f64* dates_ptr_;
    const StrikeVolatilityType* spline_coefficients_ptr_;
    const DeltaVolatilityType* deltas_volatility_ptr_;
};


template<typename Interpolation = QuadraticSplineInterpolator>
class VolatilitySurfaceProvider {
public:

    using StrikeType = f64;
    using VolatilityType = f64;

    using StrikeVolatilityType = typename Interpolation::StrikeVolatilityType;
    using DeltaVolatilityType = typename Interpolation::DeltaVolatilityType;

    using SurfaceType = VolatilitySurface<Interpolation>;
    using SurfaceHeader = typename SurfaceType::SurfaceHeader;

public:


    explicit VolatilitySurfaceProvider(const DateType& today) : today_(today), surface_ptr_(nullptr) {
    }


    [[nodiscard]] Expect<void, Error> AddPillarDelta(f64 value) {
        pillar_deltas_.push_back(value);
        return Ok();
    }

    void ErasePillarDelta(f64 value) {
        auto found_iter = std::ranges::find(pillar_deltas_, value);
        if (found_iter != pillar_deltas_.end()) {
            pillar_deltas_.erase(found_iter);
        }
    }

    Expect<void, Error> AddPillar(const DateType& date, const StrikeType strike,
                                  const VolatilityType volatility) {
        if (date < today_) {
            return ErrorDateInAPast();
        }

        strikes_.push_back(strike);
        pillars_[date][strike] = volatility;
        dates_.push_back(Period{today_, date}.Act365());

        return Ok();
    }

    [[nodiscard]] Expect<void, Error> UpdateSnapshot(const f64 spot, const Curve& domestic_curve,
                                                     const Curve& foreign_curve) noexcept {
        namespace stdr = std::ranges;

        // Sort and remove duplicates in dates
        stdr::sort(dates_);
        dates_.erase(stdr::unique(dates_).begin(), dates_.end());

        // Sort and remove duplicates in strikes
        stdr::sort(strikes_);
        strikes_.erase(stdr::unique(strikes_).begin(), strikes_.end());

        // Sort and remove duplicates in deltas
        stdr::sort(pillar_deltas_);
        pillar_deltas_.erase(stdr::unique(pillar_deltas_).begin(), pillar_deltas_.end());

        // Compute sizes
        const u64 interp_state_aligned_size =
            internal::AlignSize(Interpolation::StateRequiredMemorySize(strikes_.size()),
                                Interpolation::StateRequiredMemoryAlignment(strikes_.size()));

        const u64 delta_interp_state_aligned_size =
            internal::AlignSize(Interpolation::StateRequiredMemorySize(pillar_deltas_.size()),
                                Interpolation::StateRequiredMemoryAlignment(pillar_deltas_.size()));

        constexpr u64 header_size = sizeof(SurfaceHeader);

        const u64 strikes_size_bytes = strikes_.size() * sizeof(f64);
        const u64 pillar_deltas_size_bytes = pillar_deltas_.size() * sizeof(f64);
        const u64 dates_size_bytes = dates_.size() * sizeof(f64);

        const u64 interp_states_size = dates_.size() * interp_state_aligned_size;
        const u64 delta_interp_states_size = dates_.size() * delta_interp_state_aligned_size;

        // Compute aligned offsets
        const u64 strikes_offset = internal::AlignToCacheLine(header_size);
        const u64 pillar_deltas_offset = internal::AlignToCacheLine(strikes_offset + strikes_size_bytes);
        const u64 dates_offset = internal::AlignToCacheLine(pillar_deltas_offset + pillar_deltas_size_bytes);
        const u64 states_offset = internal::AlignSize(dates_offset + dates_size_bytes,
                                                      Interpolation::StateRequiredMemoryAlignment(strikes_.size()));
        const u64 delta_states_offset = internal::AlignSize(
            states_offset + interp_states_size, Interpolation::StateRequiredMemoryAlignment(pillar_deltas_.size()));
        const u64 total_size = internal::AlignToCacheLine(delta_states_offset + delta_interp_states_size);

        // Allocate buffer
        std::byte* buffer_ptr =
            static_cast<std::byte*>(cdr::AlignedAlloc(kHardwareDestructiveInterferenceSize, total_size));

        if (!buffer_ptr) [[unlikely]] {
            return ErrorNoMemory();
        }

        // Fill header
        SurfaceHeader* header = new (buffer_ptr) SurfaceHeader;

        header->today = today_;
        header->strikes_size = strikes_.size();
        header->pillar_deltas_size = pillar_deltas_.size();
        header->dates_size = dates_.size();

        header->reference_count.store(1, std::memory_order_relaxed);

        header->spot = spot;

        header->strikes_byte_offset = strikes_offset;
        header->pillar_deltas_byte_offset = pillar_deltas_offset;
        header->dates_byte_offset = dates_offset;
        header->states_byte_offset = states_offset;
        header->deltas_byte_offset = delta_states_offset;
        header->total_size_in_bytes = total_size;

        // Pointers to payload areas
        f64* strikes_ptr = reinterpret_cast<f64*>(buffer_ptr + strikes_offset);

        f64* pillar_deltas_ptr = reinterpret_cast<f64*>(buffer_ptr + pillar_deltas_offset);

        f64* dates_ptr = reinterpret_cast<f64*>(buffer_ptr + dates_offset);

        StrikeVolatilityType* states_ptr = reinterpret_cast<StrikeVolatilityType*>(buffer_ptr + states_offset);

        DeltaVolatilityType* delta_states_ptr = reinterpret_cast<DeltaVolatilityType*>(buffer_ptr + delta_states_offset);

        // Copy strikes and dates
        std::memcpy(strikes_ptr, strikes_.data(), strikes_size_bytes);
        std::memcpy(pillar_deltas_ptr, pillar_deltas_.data(), pillar_deltas_size_bytes);
        std::memcpy(dates_ptr, dates_.data(), dates_size_bytes);

        // Fill volatility matrix
        const u64 strikes_size = strikes_.size();
        const u64 dates_size = dates_.size();

        auto pillars_iter = pillars_.cbegin();

        std::vector<f64> row_raw_vols(strikes_size);
        std::vector<f64> delta_raw_vols(pillar_deltas_.size());

        for (u64 date_idx = 0; date_idx < dates_size; ++date_idx, ++pillars_iter) {
            const auto& date_strikes = pillars_iter->second;

            for (u64 strike_idx = 0; strike_idx < strikes_size; ++strike_idx) {
                auto pillar_iter = date_strikes.lower_bound(strikes_[strike_idx]);

                f64 output_value = 0.0;

                if (pillar_iter == date_strikes.end()) {
                    output_value = std::prev(pillar_iter)->second;
                } else if (pillar_iter->first == strikes_[strike_idx] || pillar_iter == date_strikes.begin()) {
                    output_value = pillar_iter->second;
                } else {
                    auto [prev_strike, prev_vol] = *std::prev(pillar_iter);
                    auto [curr_strike, curr_vol] = *pillar_iter;

                    output_value = Lerp(strikes_[strike_idx], prev_strike, prev_vol, curr_strike, curr_vol);
                }

                row_raw_vols[strike_idx] = output_value;
            }

            Interpolation::InitState(&states_ptr[date_idx * interp_state_aligned_size], strikes_, row_raw_vols)
                .OrCrashProgram();

            // --- Precompute delta smile ---
            const DateType target_date = pillars_iter->first;

            const f64 T = dates_[date_idx];
            const u64 deltas_size = pillar_deltas_.size();

            const f64 rd =
                domestic_curve.Interpolated<Linear>(target_date, domestic_curve.Calendar(), domestic_curve.GetJurisdiction())
                    .Fraction();

            const f64 rf =
                foreign_curve.Interpolated<Linear>(target_date, foreign_curve.Calendar(), foreign_curve.GetJurisdiction())
                    .Fraction();

            auto EvaluateLocalSpline = [&](f64 K) -> f64 {
                if (strikes_size == 0) {
                    return 0.0;
                }

                if (K <= strikes_.front()) {
                    return Interpolation::Evaluate(&states_ptr[date_idx * strikes_size], strikes_, strikes_.front())
                        .OrCrashProgram();
                }

                if (K >= strikes_.back()) {
                    return Interpolation::Evaluate(&states_ptr[date_idx * strikes_size], strikes_, strikes_.back())
                        .OrCrashProgram();
                }

                return Interpolation::Evaluate(&states_ptr[date_idx * strikes_size], strikes_, K).OrCrashProgram();
            };

            const f64 max_achievable_delta = std::exp(-rf * T);

            for (u64 d_idx = 0; d_idx < deltas_size; ++d_idx) {
                f64 target_delta = pillar_deltas_[d_idx];

                // Clamp impossible deltas
                if (std::abs(target_delta) >= max_achievable_delta) {
                    const f64 sign = (target_delta > 0.0) ? 1.0 : -1.0;
                    target_delta = sign * (max_achievable_delta - 1e-5);
                }

                const OptionType type = (target_delta > 0.0) ? OptionType::CALL : OptionType::PUT;

                // Initial ATM vol guess
                const f64 atm_vol = std::max(EvaluateLocalSpline(spot), 1e-4);

                // Black-Scholes closed-form strike guess
                const f64 guess_K = FxOptionStrikeFromDelta(spot, rd, rf, atm_vol, T, type, target_delta);

                auto objective = [&](f64 K) -> f64 {
                    const f64 vol = std::max(EvaluateLocalSpline(K), 1e-4);

                    return FxOptionDelta(spot, K, rd, rf, vol, T, type) - target_delta;
                };

                const f64 left_bound = strikes_.front();
                const f64 right_bound = strikes_.back();

                f64 solved_K = guess_K;

                auto root_search = FindRoot(objective, left_bound, right_bound, guess_K);

                if (root_search.Succeed()) {
                    solved_K = root_search.Value();
                }

                const f64 target_vol = std::max(EvaluateLocalSpline(solved_K), 0.0);

                delta_raw_vols[d_idx] = target_vol;
            }

            Interpolation::InitState(&delta_states_ptr[date_idx * delta_interp_state_aligned_size], pillar_deltas_, delta_raw_vols)
                .OrCrashProgram();
        }

        // Swap the new surface
        void* old_surface_ptr = surface_ptr_.exchange(buffer_ptr, std::memory_order_acq_rel);

        if (old_surface_ptr) {
            SurfaceHeader* old_header = static_cast<SurfaceHeader*>(old_surface_ptr);

            if (old_header->reference_count.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                cdr::AlignedFree(old_surface_ptr);
            }
        }

        return Ok();
    }

    Expect<SurfaceType, Error> ProvideSnapshot() const noexcept {
        void* data_ptr = surface_ptr_.load(std::memory_order_acquire);
        if (!data_ptr) [[unlikely]] {
            return ErrorNoData();
        }
        static_cast<SurfaceHeader*>(data_ptr)->reference_count.fetch_add(1, std::memory_order_acq_rel);

        return cdr::Ok<SurfaceType>(std::in_place, data_ptr);
    }

    virtual ~VolatilitySurfaceProvider() {
        void* surface_ptr = surface_ptr_.exchange(nullptr, std::memory_order_acq_rel);
        if (!surface_ptr) [[unlikely]] {
            return;
        }

        SurfaceHeader* header = static_cast<SurfaceHeader*>(surface_ptr);
        const u64 old_refs = header->reference_count.fetch_sub(1, std::memory_order_acq_rel);
        if (old_refs == 1) {
            cdr::AlignedFree(surface_ptr);
        }
    }

private:

    std::map<DateType, std::map<StrikeType, VolatilityType>> pillars_;
    std::vector<StrikeType> strikes_;
    std::vector<f64> pillar_deltas_;
    std::vector<f64> dates_;
    DateType today_;

    std::atomic<void*> surface_ptr_;

};

} // namespace cdr
