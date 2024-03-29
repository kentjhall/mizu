// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
//
// Adapted by Kent Hall for mizu on Horizon Linux.

#pragma once

#include <array>
#include <type_traits>

#include "video_core/surface.h"
#include "video_core/texture_cache/types.h"
#include "video_core/textures/texture.h"

namespace VideoCommon {

using Tegra::Texture::SwizzleSource;
using Tegra::Texture::TICEntry;
using VideoCore::Surface::PixelFormat;

/// Properties used to determine a image view
struct ImageViewInfo {
    explicit ImageViewInfo() noexcept = default;
    explicit ImageViewInfo(const TICEntry& config, s32 base_layer) noexcept;
    explicit ImageViewInfo(ImageViewType type, PixelFormat format,
                           SubresourceRange range = {}) noexcept;

    auto operator<=>(const ImageViewInfo&) const noexcept = default;

    [[nodiscard]] bool IsRenderTarget() const noexcept;

    [[nodiscard]] std::array<SwizzleSource, 4> Swizzle() const noexcept {
        return std::array{
            static_cast<SwizzleSource>(x_source),
            static_cast<SwizzleSource>(y_source),
            static_cast<SwizzleSource>(z_source),
            static_cast<SwizzleSource>(w_source),
        };
    }

    ImageViewType type{};
    PixelFormat format{};
    SubresourceRange range;
    u8 x_source = static_cast<u8>(SwizzleSource::R);
    u8 y_source = static_cast<u8>(SwizzleSource::G);
    u8 z_source = static_cast<u8>(SwizzleSource::B);
    u8 w_source = static_cast<u8>(SwizzleSource::A);
};
static_assert(std::has_unique_object_representations_v<ImageViewInfo>);

} // namespace VideoCommon
