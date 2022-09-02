// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
//
// Adapted by Kent Hall for mizu on Horizon Linux.

#pragma once

#include <memory>
#include "core/frontend/emu_window.h"
#include "video_core/renderer_base.h"

namespace Core::Frontend {
class EmuWindow;
}

namespace Tegra {
class GPU;
}

namespace Service {
template <typename T>
class Shared;
};

namespace VideoCore {

class RendererBase;

std::unique_ptr<VideoCore::RendererBase> CreateRenderer(
    Tegra::GPU& gpu, std::unique_ptr<Core::Frontend::GraphicsContext> context);

u16 GetResolutionScaleFactor(const RendererBase& renderer);

} // namespace VideoCore
