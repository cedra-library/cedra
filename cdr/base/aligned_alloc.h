#pragma once

#include <cdr/types/integers.h>

#include <cstdlib>
#include <new>

#ifdef _MSC_VER
#include <malloc.h>
#endif  // _MSC_VER

namespace cdr {

inline void* AlignedAlloc(u64 alignment, u64 size) noexcept {
#ifdef _MSC_VER
    // Windows
    return _aligned_malloc(size, alignment);
#else   // _MSC_VER
    // unix
    return std::aligned_alloc(alignment, size);
#endif  // _MSC_VER
}

inline void AlignedFree(void* ptr) {
#if defined(_MSC_VER)
    _aligned_free(ptr);
#else   // _MSC_VER
    free(ptr);
#endif  // _MSC_VER
}

}  // namespace cdr