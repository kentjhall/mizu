// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
//
// Adapted by Kent Hall for mizu on Horizon Linux.

#include <memory>

#include "common/logging/log.h"
#include "common/settings.h"
#include "core/core.h"
#include "core/hle/service/service.h"
#include "video_core/renderer_base.h"
#include "video_core/renderer_opengl/renderer_opengl.h"
#include "video_core/renderer_vulkan/renderer_vulkan.h"
#include "video_core/video_core.h"

namespace VideoCore {

std::unique_ptr<VideoCore::RendererBase> CreateRenderer(
    Tegra::GPU& gpu, std::unique_ptr<Core::Frontend::GraphicsContext> context) {
    switch (Settings::values.renderer_backend.GetValue()) {
    case Settings::RendererBackend::OpenGL:
        return std::make_unique<OpenGL::RendererOpenGL>(gpu, std::move(context));
    case Settings::RendererBackend::Vulkan:
        return std::make_unique<Vulkan::RendererVulkan>(gpu, std::move(context));
    default:
        return nullptr;
    }
}

u16 GetResolutionScaleFactor(const RendererBase& renderer) {
    return static_cast<u16>(
        Settings::values.resolution_factor.GetValue() != 0
            ? Settings::values.resolution_factor.GetValue()
            : renderer.GetEmuWindow().GetFramebufferLayout().GetScalingRatio());
}

} // namespace VideoCore
