// unPAKer - Game Resource Archive Extractor
// Copyright (c) 2026 mxtherfxcker and contributors
// Licensed under MIT License

#pragma once

#include <iostream>
#include <atomic>
#include <mutex>
#include <iomanip>
#include <sstream>

namespace unpaker {

struct MemoryStats {
    std::atomic<size_t> total_allocated{0};
    std::atomic<size_t> total_freed{0};
    std::atomic<size_t> peak_memory{0};
    std::atomic<size_t> allocation_count{0};
    std::atomic<size_t> deallocation_count{0};

    size_t get_current_memory() const {
        size_t allocated = total_allocated.load(std::memory_order_relaxed);
        size_t freed = total_freed.load(std::memory_order_relaxed);
        return (allocated > freed) ? (allocated - freed) : 0;
    }

    void update_peak() {
        size_t current = get_current_memory();
        size_t current_peak = peak_memory.load(std::memory_order_relaxed);
        while (current > current_peak && !peak_memory.compare_exchange_weak(
            current_peak, current, std::memory_order_relaxed, std::memory_order_relaxed)) {
            current_peak = peak_memory.load(std::memory_order_relaxed);
        }
    }
};

extern MemoryStats g_memory_stats;


inline void unpaker_memory_checkpoint(const char* label = "Checkpoint") {
    size_t current = g_memory_stats.get_current_memory();
    size_t peak = g_memory_stats.peak_memory.load(std::memory_order_relaxed);
    size_t alloc_count = g_memory_stats.allocation_count.load(std::memory_order_relaxed);
    size_t dealloc_count = g_memory_stats.deallocation_count.load(std::memory_order_relaxed);

    std::cout << "[MEMORY] " << label << ": "
                              << "Current=" << std::fixed << std::setprecision(2) << (current / 1024.0) << " KB, "
                              << "Peak=" << (peak / 1024.0) << " KB, "
                              << "Allocs=" << alloc_count << ", "
                              << "Deallocs=" << dealloc_count << std::endl;
}

inline size_t unpaker_get_current_memory() {
    return g_memory_stats.get_current_memory();
}

inline size_t unpaker_get_peak_memory() {
    return g_memory_stats.peak_memory.load(std::memory_order_relaxed);
}

inline size_t unpaker_get_allocation_count() {
    return g_memory_stats.allocation_count.load(std::memory_order_relaxed);
}

inline void unpaker_reset_memory_stats() {
    g_memory_stats.total_allocated.store(0, std::memory_order_relaxed);
    g_memory_stats.total_freed.store(0, std::memory_order_relaxed);
    g_memory_stats.peak_memory.store(0, std::memory_order_relaxed);
    g_memory_stats.allocation_count.store(0, std::memory_order_relaxed);
    g_memory_stats.deallocation_count.store(0, std::memory_order_relaxed);
}

#define UNPAKER_TRACK_ALLOC(size) \
    do { \
        unpaker::g_memory_stats.total_allocated.fetch_add((size), std::memory_order_relaxed); \
        unpaker::g_memory_stats.allocation_count.fetch_add(1, std::memory_order_relaxed); \
        unpaker::g_memory_stats.update_peak(); \
    } while(0)

#define UNPAKER_TRACK_FREE(size) \
    do { \
        unpaker::g_memory_stats.total_freed.fetch_add((size), std::memory_order_relaxed); \
        unpaker::g_memory_stats.deallocation_count.fetch_add(1, std::memory_order_relaxed); \
    } while(0)

} // namespace unpaker
