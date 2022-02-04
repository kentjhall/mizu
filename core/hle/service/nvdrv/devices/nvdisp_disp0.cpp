// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/assert.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/hle/service/nvdrv/devices/nvdisp_disp0.h"
#include "core/hle/service/nvdrv/devices/nvmap.h"
#include "core/perf_stats.h"
#include "video_core/gpu.h"
#include "video_core/renderer_base.h"

namespace Service::Nvidia::Devices {

nvdisp_disp0::nvdisp_disp0(std::shared_ptr<nvmap> nvmap_dev_)
    : nvdevice_locked<nvdisp_disp0>{}, nvmap_dev{std::move(nvmap_dev_)} {}
nvdisp_disp0::~nvdisp_disp0() = default;

NvResult nvdisp_disp0::Ioctl1(DeviceFD fd, Ioctl command, const std::vector<u8>& input,
                              std::vector<u8>& output, Shared<Tegra::GPU>& gpu) {
    UNIMPLEMENTED_MSG("Unimplemented ioctl={:08X}", command.raw);
    return NvResult::NotImplemented;
}

NvResult nvdisp_disp0::Ioctl2(DeviceFD fd, Ioctl command, const std::vector<u8>& input,
                              const std::vector<u8>& inline_input, std::vector<u8>& output, Shared<Tegra::GPU>& gpu) {
    UNIMPLEMENTED_MSG("Unimplemented ioctl={:08X}", command.raw);
    return NvResult::NotImplemented;
}

NvResult nvdisp_disp0::Ioctl3(DeviceFD fd, Ioctl command, const std::vector<u8>& input,
                              std::vector<u8>& output, std::vector<u8>& inline_output, Shared<Tegra::GPU>& gpu) {
    UNIMPLEMENTED_MSG("Unimplemented ioctl={:08X}", command.raw);
    return NvResult::NotImplemented;
}

void nvdisp_disp0::OnOpen(DeviceFD fd, Shared<Tegra::GPU>& gpu) {}
void nvdisp_disp0::OnClose(DeviceFD fd, Shared<Tegra::GPU>& gpu) {}

void nvdisp_disp0::flip(u32 buffer_handle, u32 offset, u32 format, u32 width, u32 height,
                        u32 stride, NVFlinger::BufferQueue::BufferTransformFlags transform,
                        const Common::Rectangle<int>& crop_rect, Shared<Tegra::GPU>& gpu) {
    const VAddr addr = nvmap_dev->ReadLocked()->GetObjectAddress(buffer_handle);
    LOG_TRACE(Service,
              "Drawing from address {:X} offset {:08X} Width {} Height {} Stride {} Format {}",
              addr, offset, width, height, stride, format);

    const auto pixel_format = static_cast<Tegra::FramebufferConfig::PixelFormat>(format);
    const auto transform_flags = static_cast<Tegra::FramebufferConfig::TransformFlags>(transform);
    const Tegra::FramebufferConfig framebuffer{addr,   offset,       width,           height,
                                               stride, pixel_format, transform_flags, crop_rect,
                                               SharedUnlocked(gpu)->SessionPid()};

    SharedWriter(gpu)->GetPerfStats().EndSystemFrame();
    ::fprintf(stderr, "SwapBuffers\n");
    SharedUnlocked(gpu)->SwapBuffers(&framebuffer);
    SharedWriter gpu_locked(gpu);
    gpu_locked->SpeedLimiter().DoSpeedLimiting(GetGlobalTimeUs());
    gpu_locked->GetPerfStats().BeginSystemFrame();
}

} // namespace Service::Nvidia::Devices
