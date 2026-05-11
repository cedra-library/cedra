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
#include <ranges>
#include <map>
#include <vector>
#include <cstring>
#include <cdr/options/interpolation/quadratic_spline.h>
#include <cdr/options/interpolation/lerp.h>
#include <cdr/options/interpolation/flat_forward.h>


namespace cdr {


// Forward decl
template<typename Interpolator>
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

template<typename Interpolator = QuadraticSplineInterpolator>
class VolatilitySurface {
public:

    struct SurfaceHeader {
        DateType today;
        static_assert(sizeof(DateType) == sizeof(u32));

        u32 strikes_size;
        u32 dates_size;

        u32 strikes_byte_offset;
        u32 dates_byte_offset;
        u32 states_byte_offset;

        mutable std::atomic<u32> reference_count;

        size_t total_size_in_bytes;
    };

    using StrikeVolatilityType = typename Interpolator::StrikeVolatilityType;

public:

    explicit VolatilitySurface(void* incremented_base) {
        CDR_CHECK(incremented_base != nullptr) << "VolatilitySurface cannot be nullptr.";
        header_ptr_ = static_cast<SurfaceHeader*>(incremented_base); auto* base_ptr = static_cast<std::byte*>(incremented_base);
        strikes_ptr_ = reinterpret_cast<f64*>(base_ptr + header_ptr_->strikes_byte_offset);
        dates_ptr_ = reinterpret_cast<f64*>(base_ptr + header_ptr_->dates_byte_offset);
        spline_coefficients_ptr_ = reinterpret_cast<StrikeVolatilityType*>(base_ptr + header_ptr_->states_byte_offset);
    }

    VolatilitySurface(const VolatilitySurface& other) noexcept {
        other.Header().reference_count.fetch_add(1, std::memory_order_acq_rel);

        header_ptr_ = other.header_ptr_;
        strikes_ptr_ = other.strikes_ptr_;
        dates_ptr_ = other.dates_ptr_;
        spline_coefficients_ptr_ = other.spline_coefficients_ptr_;
    }


    VolatilitySurface& operator=(const VolatilitySurface& other) noexcept {
        if (&other != this) [[likely]] {
            if (other.header_ptr_) {
                other.Header().reference_count.fetch_add(1, std::memory_order_acq_rel);
            }
            this->Reclaim();

            header_ptr_ = other.header_ptr_;
            strikes_ptr_ = other.strikes_ptr_;
            dates_ptr_ = other.dates_ptr_;
            spline_coefficients_ptr_ = other.spline_coefficients_ptr_;
        }

        return *this;
    }

    VolatilitySurface(VolatilitySurface&& other) noexcept
        : header_ptr_(std::exchange(other.header_ptr_, nullptr))
        , strikes_ptr_(std::exchange(other.strikes_ptr_, nullptr))
        , dates_ptr_(std::exchange(other.dates_ptr_, nullptr))
        , spline_coefficients_ptr_(std::exchange(other.spline_coefficients_ptr_, nullptr))
    {}


    VolatilitySurface& operator=(VolatilitySurface&& other) noexcept {
        if (&other != this) [[likely]] {
            this->Reclaim();
            header_ptr_ = std::exchange(other.header_ptr_, nullptr);
            strikes_ptr_ = std::exchange(other.strikes_ptr_, nullptr);
            dates_ptr_ = std::exchange(other.dates_ptr_, nullptr);
            spline_coefficients_ptr_ = std::exchange(other.spline_coefficients_ptr_, nullptr);
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

    [[nodiscard]] cdr::Expect<f64, Error> Volatility(const DateType& date, f64 strike) const noexcept {
        if (header_ptr_ == nullptr) [[unlikely]] {
            return ErrorNoData();
        }

        // QoL constants
        const f64 target_time = Period{Today(), date}.Act365();
        const auto strikes_span = Strikes();
        const auto dates_span = Dates();
        const u32 strikes_size = header_ptr_->strikes_size;

        // Validate that date can be interpolated
        if (target_time < dates_span.front() || target_time > dates_span.back()) [[unlikely]] {
            return ErrorTimeExtrapolationNotAllowed();
        }

        // Validate that strike can be interpolated
        if (strike < strikes_span.front() || strike > strikes_span.back()) [[unlikely]] {
            return ErrorStrikeExtrapolationNotAllowed();
        }

        namespace stdr = std::ranges;

        // Obtain date on surface
        auto date_it = stdr::lower_bound(dates_span, target_time);
        u64 date_idx = internal::IndexFromIterator(dates_span.begin(), dates_span.size(), date_it);

        // Obtain strike
        auto strike_it = stdr::lower_bound(strikes_span, strike);
        u64 strike_idx = internal::IndexFromIterator(strikes_span.begin(), strikes_span.size(), strike_it);

        // Compute interpolated volatility
        auto EvaluateSpline = [&](u64 date_idx) -> f64 {
            return Interpolator::Evaluate(&spline_coefficients_ptr_[date_idx * strikes_size], strikes_span, strike)
                .OrCrashProgram();
        };

        const f64 volaility_t1 = EvaluateSpline(date_idx);

        if (dates_span[date_idx] == target_time || dates_span.size() == 1) {
            return Ok<f64>(volaility_t1);
        }

        const f64 volatility_t2 = EvaluateSpline(date_idx+1);
        const f64 result = FlatForward(target_time, dates_span[date_idx], volaility_t1, dates_span[date_idx+1], volatility_t2);

        return Ok(result);

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

private:
    const SurfaceHeader* header_ptr_;
    const f64* strikes_ptr_;
    const f64* dates_ptr_;
    const StrikeVolatilityType* spline_coefficients_ptr_;
};


template<typename Interpolator = QuadraticSplineInterpolator>
class VolatilitySurfaceProvider {
public:

    using StrikeType = f64;
    using VolatilityType = f64;

    using StrikeVolatilityType = typename Interpolator::StrikeVolatilityType;
    using SurfaceType = VolatilitySurface<Interpolator>;
    using SurfaceHeader = typename SurfaceType::SurfaceHeader;
public:


    explicit VolatilitySurfaceProvider(const DateType& today) : today_(today), surface_ptr_(nullptr) {
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

    Expect<void, Error> UpdateSnapshot() noexcept {
        namespace stdr = std::ranges;

        // Sort and remove duplicates in dates
        stdr::sort(dates_);
        dates_.erase(stdr::unique(dates_).begin(), dates_.end());

        // Sort and remove duplicates in strikes
        stdr::sort(strikes_);
        strikes_.erase(stdr::unique(strikes_).begin(), strikes_.end());

        // Compute sizes
        const u64 interp_state_aligned_size =
            internal::AlignSize(Interpolator::StateRequiredMemorySize(strikes_.size()),
                                Interpolator::StateRequiredMemoryAlignment(strikes_.size()));
        constexpr u64 header_size = sizeof(SurfaceHeader);
        const u64 strikes_size_bytes = strikes_.size() * sizeof(f64);
        const u64 dates_size_bytes = dates_.size() * sizeof(f64);
        const u64 interp_states_size = dates_.size() * interp_state_aligned_size;

        // Compute aligned offsets
        const u64 strikes_offset = internal::AlignToCacheLine(header_size);
        const u64 dates_offset = internal::AlignToCacheLine(strikes_offset + strikes_size_bytes);
        const u64 states_offset = internal::AlignSize(dates_offset + dates_size_bytes,
                                                      Interpolator::StateRequiredMemoryAlignment(strikes_.size()));
        const u64 total_size = internal::AlignToCacheLine(states_offset + interp_states_size);

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
        header->dates_size = dates_.size();
        header->reference_count.store(1, std::memory_order_relaxed);

        header->strikes_byte_offset = strikes_offset;
        header->dates_byte_offset = dates_offset;
        header->states_byte_offset = states_offset;
        header->total_size_in_bytes = total_size;

        // Pointers to payload areas
        f64* strikes_ptr = reinterpret_cast<f64*>(buffer_ptr + strikes_offset);
        f64* dates_ptr = reinterpret_cast<f64*>(buffer_ptr + dates_offset);
        StrikeVolatilityType* states_ptr = reinterpret_cast<StrikeVolatilityType*>(buffer_ptr + states_offset);

        // Copy strikes and dates
        std::memcpy(strikes_ptr, strikes_.data(), strikes_size_bytes);
        std::memcpy(dates_ptr, dates_.data(), dates_size_bytes);

        // Fill volatility matrix (unchanged logic)
        const u64 strikes_size = strikes_.size();
        const u64 dates_size = dates_.size();
        auto pillars_iter = pillars_.cbegin();

        std::vector<f64> row_raw_vols(strikes_size);

        for (u64 date_idx = 0; date_idx < dates_size; ++date_idx, ++pillars_iter) {
            // --- Grid Projection ---
            // Market quotes (pillars) may not exist for every strike in our global grid.
            // We project available market data onto the global grid using linear interpolation.
            const auto& date_strikes = pillars_iter->second;
            for (u64 strike_idx = 0; strike_idx < strikes_size; ++strike_idx) {
                auto pillar_iter = date_strikes.lower_bound(strikes_[strike_idx]);

                f64 output_value = 0.0;
                if (pillar_iter == date_strikes.end()) {
                    // Strike is beyond the last known market pillar: apply flat right-extrapolation
                    output_value = std::prev(pillar_iter)->second;
                } else if (pillar_iter->first == strikes_[strike_idx] || pillar_iter == date_strikes.begin()) {
                    // Exact match or strike is before the first market pillar: apply flat left-extrapolation
                    output_value = pillar_iter->second;
                } else {
                    // Strike is between two market pillars: linearly interpolate to find the anchor point
                    auto [prev_strike, prev_vol] = *std::prev(pillar_iter);
                    auto [curr_strike, curr_vol] = *pillar_iter;
                    output_value = Lerp(strikes_[strike_idx], prev_strike, prev_vol, curr_strike, curr_vol);
                }
                // Store the intermediate value in the temporary row buffer
                row_raw_vols[strike_idx] = output_value;
            }

            Interpolator::InitState(&states_ptr[date_idx * interp_state_aligned_size], strikes_, row_raw_vols)
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
    std::vector<f64> dates_;
    DateType today_;

    std::atomic<void*> surface_ptr_;

};

} // namespasce cdr
