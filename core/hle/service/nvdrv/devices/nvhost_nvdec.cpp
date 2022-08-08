// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
//
// Adapted by Kent Hall for mizu on Horizon Linux.

#include "common/assert.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/hle/service/nvdrv/devices/nvhost_nvdec.h"
#include "video_core/memory_manager.h"
#include "video_core/renderer_base.h"

namespace Service::Nvidia::Devices {

nvhost_nvdec::nvhost_nvdec(std::shared_ptr<nvmap> nvmap_dev_,
                           SyncpointManager& syncpoint_manager_)
    : nvhost_nvdec_common{std::move(nvmap_dev_), syncpoint_manager_} {}
nvhost_nvdec::~nvhost_nvdec() = default;

NvResult nvhost_nvdec::Ioctl1(DeviceFD fd, Ioctl command, const std::vector<u8>& input,
                              std::vector<u8>& output, Shared<Tegra::GPU>& gpu) {
    switch (command.group) {
    case 0x0:
        switch (command.cmd) {
        case 0x1:
            return Submit(input, output, gpu);
        case 0x2:
            return GetSyncpoint(input, output, gpu);
        case 0x3:
            return GetWaitbase(input, output);
        case 0x7:
            return SetSubmitTimeout(input, output);
        case 0x9:
            return MapBuffer(input, output, gpu);
        case 0xa:
            return UnmapBuffer(input, output);
        default:
            break;
        }
        break;
    case 'H':
        switch (command.cmd) {
        case 0x1:
            return SetNVMAPfd(input);
        default:
            break;
        }
        break;
    }

    UNIMPLEMENTED_MSG("Unimplemented ioctl={:08X}", command.raw);
    return NvResult::NotImplemented;
}

NvResult nvhost_nvdec::Ioctl2(DeviceFD fd, Ioctl command, const std::vector<u8>& input,
                              const std::vector<u8>& inline_input, std::vector<u8>& output, Shared<Tegra::GPU>& gpu) {
    UNIMPLEMENTED_MSG("Unimplemented ioctl={:08X}", command.raw);
    return NvResult::NotImplemented;
}

NvResult nvhost_nvdec::Ioctl3(DeviceFD fd, Ioctl command, const std::vector<u8>& input,
                              std::vector<u8>& output, std::vector<u8>& inline_output, Shared<Tegra::GPU>& gpu) {
    UNIMPLEMENTED_MSG("Unimplemented ioctl={:08X}", command.raw);
    return NvResult::NotImplemented;
}

void nvhost_nvdec::OnOpen(DeviceFD fd, Shared<Tegra::GPU>& gpu) {}

void nvhost_nvdec::OnClose(DeviceFD fd, Shared<Tegra::GPU>& gpu) {
    LOG_INFO(Service_NVDRV, "NVDEC video stream ended");
    SharedWriter(gpu)->ClearCdmaInstance();
}

} // namespace Service::Nvidia::Devices
