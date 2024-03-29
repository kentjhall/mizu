// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
//
// Adapted by Kent Hall for mizu on Horizon Linux.

#pragma once

#include <array>
#include <atomic>

#include "common/common_types.h"
#include "video_core/rasterizer_interface.h"

namespace Core::Memory {
class Memory;
}

namespace VideoCore {

/// Implements the shared part in GPU accelerated rasterizers in RasterizerInterface.
class RasterizerAccelerated : public RasterizerInterface {
public:
    explicit RasterizerAccelerated(Tegra::GPU& gpu_);
    ~RasterizerAccelerated() override;

    void UpdatePagesCachedCount(VAddr addr, u64 size, int delta) override;

private:
    class CacheEntry final {
    public:
        CacheEntry() = default;

        std::atomic_uint16_t& Count(std::size_t page) {
            return values[page & 3];
        }

        const std::atomic_uint16_t& Count(std::size_t page) const {
            return values[page & 3];
        }

    private:
        std::array<std::atomic_uint16_t, 4> values{};
    };
    static_assert(sizeof(CacheEntry) == 8, "CacheEntry should be 8 bytes!");

    std::array<CacheEntry, 0x2000000> cached_pages;
};

} // namespace VideoCore
