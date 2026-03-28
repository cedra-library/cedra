#include <cdr/calendar/date.h>
#include <cdr/options/volatility.h>
#include <stdlib.h>

#include <cstring>
#include <new>  // for std::hardware_destructive_interference_size

namespace stdr = std::ranges;

namespace cdr {

VolatilitySurface::VolatilitySurface(void* incremented_base) {
    CDR_CHECK(incremented_base != nullptr) << "VolatilitySurface cannot be nullptr.";
    header_ptr_ = static_cast<SurfaceHeader*>(incremented_base);
    auto* base_ptr = static_cast<std::byte*>(incremented_base);
    strikes_ptr_ = reinterpret_cast<f64*>(base_ptr + header_ptr_->strikes_byte_offset);
    dates_ptr_ = reinterpret_cast<f64*>(base_ptr + header_ptr_->dates_byte_offset);
    volatility_ptr_ = reinterpret_cast<f64*>(base_ptr + header_ptr_->volatility_byte_offset);
}

VolatilitySurface::VolatilitySurface(const VolatilitySurface& other) noexcept {
    other.Header().reference_count.fetch_add(1, std::memory_order_acq_rel);

    header_ptr_ = other.header_ptr_;
    strikes_ptr_ = other.strikes_ptr_;
    dates_ptr_ = other.dates_ptr_;
    volatility_ptr_ = other.volatility_ptr_;
}

VolatilitySurface& VolatilitySurface::operator=(const VolatilitySurface& other) noexcept {
    if (&other != this) [[likely]] {
        if (other.header_ptr_) {
            other.Header().reference_count.fetch_add(1, std::memory_order_acq_rel);
        }
        this->Reclaim();

        header_ptr_ = other.header_ptr_;
        strikes_ptr_ = other.strikes_ptr_;
        dates_ptr_ = other.dates_ptr_;
        volatility_ptr_ = other.volatility_ptr_;
    }

    return *this;
}

VolatilitySurface::VolatilitySurface(VolatilitySurface&& other) noexcept
    : header_ptr_(std::exchange(other.header_ptr_, nullptr)),
      strikes_ptr_(std::exchange(other.strikes_ptr_, nullptr)),
      dates_ptr_(std::exchange(other.dates_ptr_, nullptr)),
      volatility_ptr_(std::exchange(other.volatility_ptr_, nullptr)) {
}

VolatilitySurface& VolatilitySurface::operator=(VolatilitySurface&& other) noexcept {
    if (&other != this) [[likely]] {
        this->Reclaim();
        header_ptr_ = std::exchange(other.header_ptr_, nullptr);
        strikes_ptr_ = std::exchange(other.strikes_ptr_, nullptr);
        dates_ptr_ = std::exchange(other.dates_ptr_, nullptr);
        volatility_ptr_ = std::exchange(other.volatility_ptr_, nullptr);
    }
    return *this;
}

VolatilitySurface::~VolatilitySurface() {
    if (header_ptr_) {
        this->Reclaim();
    }
}

inline f64 Lerp(const f64 x, const f64 x1, const f64 y1, const f64 x2, const f64 y2) noexcept {
    if (std::abs(x2 - x1) < 1e-9) return y1;
    const f64 t = (x - x1) / (x2 - x1);
    return y1 + t * (y2 - y1);
}

f64 VolatilitySurface::Volatility(const DateType& date, f64 strike) const noexcept {
    if (!header_ptr_) [[unlikely]]
        return 0.0;


    const f64 target_t = Period{header_ptr_->today, date}.ActActISDA();

    auto strikes = Strikes();
    auto dates = Dates();

    auto it_date = std::lower_bound(dates.begin(), dates.end(), target_t);
    std::size_t d1 = 0, d2 = 0;

    if (it_date == dates.begin()) {
        d1 = d2 = 0;
    } else if (it_date == dates.end()) {
        d1 = d2 = dates.size() - 1;
    } else {
        d2 = std::distance(dates.begin(), it_date);
        d1 = d2 - 1;
    }

    auto it_strike = std::lower_bound(strikes.begin(), strikes.end(), strike);
    std::size_t s1 = 0, s2 = 0;

    if (it_strike == strikes.begin()) {
        s1 = s2 = 0;
    } else if (it_strike == strikes.end()) {
        s1 = s2 = strikes.size() - 1;
    } else {
        s2 = std::distance(strikes.begin(), it_strike);
        s1 = s2 - 1;
    }
    auto get_vol = [&](std::size_t d_idx, std::size_t s_idx) {
        return volatility_ptr_[d_idx * header_ptr_->strikes_size + s_idx];
    };

    f64 vol_at_d1 = Lerp(strike, strikes[s1], get_vol(d1, s1), strikes[s2], get_vol(d1, s2));
    f64 vol_at_d2 = Lerp(strike, strikes[s1], get_vol(d2, s1), strikes[s2], get_vol(d2, s2));

    return Lerp(target_t, dates[d1], vol_at_d1, dates[d2], vol_at_d2);
}

bool VolatilitySurface::Reclaim() noexcept {
    if (!header_ptr_) {
        return false;
    }

    if (const std::size_t remaining = header_ptr_->reference_count.fetch_sub(1, std::memory_order_acq_rel);
        remaining == 1) {
        free(const_cast<SurfaceHeader*>(header_ptr_));
        header_ptr_ = nullptr;
        return true;
    }

    header_ptr_ = nullptr;
    return false;
}

Expect<void, Error> VolatilitySurfaceProvider::AddPillar(const DateType& date, const StrikeType strike,
                                                         const VolatilityType volatility) {
    if (date < today_) {
        return ErrorDateInAPast();
    }

    strikes_.push_back(strike);
    pillars_[date][strike] = volatility;
    dates_.push_back(Period{today_, date}.ActActISDA());

    return Ok();
}

inline std::size_t AlignToCacheLine(const std::size_t size) noexcept {
    static constexpr std::size_t cache_line_size = std::hardware_destructive_interference_size - 1;
    return (size + cache_line_size) & ~cache_line_size;
}

[[nodiscard]] Expect<void, Error> VolatilitySurfaceProvider::UpdateSnapshot() noexcept {
    // Sort and remove duplicates in dates
    stdr::sort(dates_);
    dates_.erase(stdr::unique(dates_).begin(), dates_.end());

    // Sort and remove duplicates in strikes
    stdr::sort(strikes_);
    strikes_.erase(stdr::unique(strikes_).begin(), strikes_.end());

    // Precompute offsets and sizes
    constexpr std::size_t k_surface_header_size_bytes = sizeof(VolatilitySurface::SurfaceHeader);
    const std::size_t k_dates_size_bytes = dates_.size() * sizeof(f64);
    const std::size_t k_strikes_size_bytes = strikes_.size() * sizeof(f64);

    const std::size_t k_whole_size_bytes =
        AlignToCacheLine(k_surface_header_size_bytes + k_strikes_size_bytes + k_dates_size_bytes +
                         dates_.size() * strikes_.size() * sizeof(f64));

    // Allocate new buffer
    std::byte* buffer_ptr =
        static_cast<std::byte*>(std::aligned_alloc(std::hardware_destructive_interference_size, k_whole_size_bytes));

    if (!buffer_ptr) [[unlikely]] {
        return ErrorNoMemory();
    }

    // Fill Header
    VolatilitySurface::SurfaceHeader* header = new (buffer_ptr) VolatilitySurface::SurfaceHeader;
    header->magic = VolatilitySurface::kMagicNumber;
    header->today = today_;
    header->strikes_size = strikes_.size();
    header->dates_size = dates_.size();
    header->reference_count.store(1, std::memory_order_relaxed);

    header->strikes_byte_offset = AlignToCacheLine(sizeof(VolatilitySurface::SurfaceHeader));
    header->dates_byte_offset = AlignToCacheLine(header->strikes_byte_offset + k_strikes_size_bytes);
    header->volatility_byte_offset = AlignToCacheLine(header->dates_byte_offset + k_dates_size_bytes);

    header->total_size_in_bytes = k_whole_size_bytes;

    // Acquire pointers to payload buffers
    f64* strikes_ptr = reinterpret_cast<f64*>(buffer_ptr + header->strikes_byte_offset);
    f64* dates_ptr = reinterpret_cast<f64*>(buffer_ptr + header->dates_byte_offset);
    f64* volatility_matrix_ptr = reinterpret_cast<f64*>(buffer_ptr + header->volatility_byte_offset);

    // Fill strikes and dates
    std::memcpy(strikes_ptr, strikes_.data(), strikes_.size() * sizeof(f64));
    std::memcpy(dates_ptr, dates_.data(), dates_.size() * sizeof(f64));

    // Fill volatility matrix
    {
        const std::size_t strikes_size = strikes_.size();
        const std::size_t dates_size = dates_.size();

        auto pillars_iter = pillars_.cbegin();

        for (std::size_t date_idx = 0; date_idx < dates_size; ++date_idx, ++pillars_iter) {
            const std::map<StrikeType, VolatilityType>& date_strikes = pillars_iter->second;

            for (std::size_t strike_idx = 0; strike_idx < strikes_size; ++strike_idx) {
                auto pillar_iter = date_strikes.lower_bound(strikes_[strike_idx]);
                const std::size_t k_matrix_output_idx = strikes_size * date_idx + strike_idx;

                f64 output_value = 0;

                if (pillar_iter == date_strikes.end()) [[unlikely]] {
                    output_value = std::prev(pillar_iter)->second;
                } else if (pillar_iter->first == strikes_[strike_idx]) [[unlikely]] {
                    output_value = pillar_iter->second;
                } else if (pillar_iter == date_strikes.begin()) [[unlikely]] {
                    output_value = pillar_iter->second;
                } else [[likely]] {
                    auto [prev_strike, prev_vol] = *std::prev(pillar_iter);
                    auto [curr_strike, curr_vol] = *pillar_iter;
                    output_value = Lerp(strikes_[strike_idx], prev_strike, prev_vol, curr_strike, curr_vol);
                }

                volatility_matrix_ptr[k_matrix_output_idx] = output_value;
            }
        }
    }

    void* old_surface_ptr = surface_ptr_.exchange(buffer_ptr, std::memory_order_acq_rel);

    if (!old_surface_ptr) [[unlikely]] {
        return Ok();
    }

    VolatilitySurface::SurfaceHeader* old_surface_header_ptr =
        static_cast<VolatilitySurface::SurfaceHeader*>(old_surface_ptr);
    std::size_t remaining = old_surface_header_ptr->reference_count.fetch_sub(1, std::memory_order_acq_rel);

    if (remaining == 1) {
        free(old_surface_ptr);
    }

    return Ok();
}

Expect<VolatilitySurface, Error> VolatilitySurfaceProvider::ProvideSnapshot() const noexcept {
    void* data_ptr = surface_ptr_.load(std::memory_order_acquire);
    if (!data_ptr) [[unlikely]] {
        return ErrorNoData();
    }
    static_cast<VolatilitySurface::SurfaceHeader*>(data_ptr)->reference_count.fetch_add(1, std::memory_order_acq_rel);

    return cdr::Ok<VolatilitySurface>(std::in_place, data_ptr);
}

}  // namespace cdr