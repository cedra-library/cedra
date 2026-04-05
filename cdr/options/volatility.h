#pragma once

#include <cdr/curve/curve.h>
#include <cdr/types/errors.h>
#include <cdr/types/expect.h>

#include <atomic>
#include <span>

namespace cdr {

class VolatilitySurfaceProvider;

// TODO: RCU and memory reclamation while swapping atomic pointers
class VolatilitySurface {
public:
    friend class VolatilitySurfaceProvider;

    static constexpr u32 kMagicNumber = 0xE1A0F12E;

    struct SurfaceHeader {
        u32 magic_number;
        DateType today;
        static_assert(sizeof(DateType) == sizeof(u32));

        u32 strikes_size;
        u32 dates_size;

        u32 strikes_byte_offset;
        u32 dates_byte_offset;
        u32 volatility_byte_offset;

        mutable std::atomic<u32> reference_count;

        size_t total_size_in_bytes;
    };

public:
    explicit VolatilitySurface(void* incremented_base);

    VolatilitySurface(const VolatilitySurface&) noexcept;
    VolatilitySurface& operator=(const VolatilitySurface&) noexcept;

    VolatilitySurface(VolatilitySurface&&) noexcept;
    VolatilitySurface& operator=(VolatilitySurface&&) noexcept;
    ~VolatilitySurface();

    [[nodiscard]] const SurfaceHeader& Header() const {
        return *header_ptr_;
    }

    [[nodiscard]] std::span<const f64> Strikes() const noexcept {
        return {strikes_ptr_, header_ptr_->strikes_size};
    }

    [[nodiscard]] std::span<const f64> Dates() const noexcept {
        return {dates_ptr_, header_ptr_->dates_size};
    }

    [[nodiscard]] f64 Volatility(const DateType& date, f64 strike) const noexcept;

private:

    [[maybe_unused]] bool Reclaim() noexcept;

private:
    const SurfaceHeader* header_ptr_;
    const f64* strikes_ptr_;
    const f64* dates_ptr_;
    const f64* volatility_ptr_;
};


class VolatilitySurfaceProvider {
public:
    using StrikeType = f64;
    using VolatilityType = f64;

public:

    explicit VolatilitySurfaceProvider(const DateType& today) : today_(today), surface_ptr_(nullptr) {
    }

    [[nodiscard]] Expect<void, Error> AddPillar(const DateType& date, const StrikeType strike,
                                                const VolatilityType volatility);

    [[nodiscard]] Expect<VolatilitySurface, Error> ProvideSnapshot() const noexcept;

    [[nodiscard]] Expect<void, Error> UpdateSnapshot() noexcept;

    ~VolatilitySurfaceProvider();
private:
    std::map<DateType, std::map<StrikeType, VolatilityType>> pillars_;
    std::vector<StrikeType> strikes_;
    std::vector<f64> dates_;
    DateType today_;

    std::atomic<void*> surface_ptr_;
};

}  // namespace cdr