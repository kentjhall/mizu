// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
//
// Adapted by Kent Hall for mizu on Horizon Linux.

#include <utility>

#include <fmt/format.h>
#include "core/core.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/service/sm/sm.h"
#include "core/hle/service/nvdrv/devices/nvdevice.h"
#include "core/hle/service/nvdrv/devices/nvdisp_disp0.h"
#include "core/hle/service/nvdrv/devices/nvhost_as_gpu.h"
#include "core/hle/service/nvdrv/devices/nvhost_ctrl.h"
#include "core/hle/service/nvdrv/devices/nvhost_ctrl_gpu.h"
#include "core/hle/service/nvdrv/devices/nvhost_gpu.h"
#include "core/hle/service/nvdrv/devices/nvhost_nvdec.h"
#include "core/hle/service/nvdrv/devices/nvhost_nvjpg.h"
#include "core/hle/service/nvdrv/devices/nvhost_vic.h"
#include "core/hle/service/nvdrv/devices/nvmap.h"
#include "core/hle/service/nvdrv/nvdrv.h"
#include "core/hle/service/nvdrv/nvdrv_interface.h"
#include "core/hle/service/nvdrv/nvmemp.h"
#include "core/hle/service/nvdrv/syncpoint_manager.h"
#include "core/hle/service/nvflinger/nvflinger.h"

namespace Service::Nvidia {

void InstallInterfaces() {
    auto module_ = std::make_shared<Shared<Module>>();
    MakeService<NVDRV>(module_, "nvdrv");
    MakeService<NVDRV>(module_, "nvdrv:a");
    MakeService<NVDRV>(module_, "nvdrv:s");
    MakeService<NVDRV>(module_, "nvdrv:t");
    MakeService<NVMEMP>();
    SharedWriter(nv_flinger)->SetNVDrvInstance(module_);
}

Module::Module()
    : syncpoint_manager{} {
    KernelHelpers::SetupServiceContext("nvdrv");
    for (u32 i = 0; i < MaxNvEvents; i++) {
        SharedUnlocked ei_unlocked(events_interface);
        ei_unlocked->events[i].event =
            KernelHelpers::CreateEvent(fmt::format("NVDRV::NvEvent_{}", i));
        ei_unlocked->status[i] = EventState::Free;
        ei_unlocked->registered[i] = false;
    }
    auto nvmap_dev = std::make_shared<Devices::nvmap>();
    devices["/dev/nvhost-as-gpu"] = std::make_shared<Devices::nvhost_as_gpu>(nvmap_dev);
    devices["/dev/nvhost-gpu"] =
        std::make_shared<Devices::nvhost_gpu>(nvmap_dev, syncpoint_manager);
    devices["/dev/nvhost-ctrl-gpu"] = std::make_shared<Devices::nvhost_ctrl_gpu>();
    devices["/dev/nvmap"] = nvmap_dev;
    devices["/dev/nvdisp_disp0"] = std::make_shared<Devices::nvdisp_disp0>(nvmap_dev);
    devices["/dev/nvhost-ctrl"] =
        std::make_shared<Devices::nvhost_ctrl>(events_interface, syncpoint_manager);
    devices["/dev/nvhost-nvdec"] =
        std::make_shared<Devices::nvhost_nvdec>(nvmap_dev, syncpoint_manager);
    devices["/dev/nvhost-nvjpg"] = std::make_shared<Devices::nvhost_nvjpg>();
    devices["/dev/nvhost-vic"] =
        std::make_shared<Devices::nvhost_vic>(nvmap_dev, syncpoint_manager);
}

Module::~Module() {
    for (u32 i = 0; i < MaxNvEvents; i++) {
        KernelHelpers::CloseEvent(SharedReader(events_interface)->events[i].event);
    }
}

NvResult Module::VerifyFD(DeviceFD fd, Shared<Tegra::GPU>& gpu) const {
    if (fd < 0) {
        LOG_ERROR(Service_NVDRV, "Invalid DeviceFD={}!", fd);
        return NvResult::InvalidState;
    }

    if (open_files.find(fd) == open_files.end()) {
        LOG_ERROR(Service_NVDRV, "Could not find DeviceFD={}!", fd);
        return NvResult::NotImplemented;
    }

    return NvResult::Success;
}

DeviceFD Module::Open(const std::string& device_name, Shared<Tegra::GPU>& gpu) {
    if (devices.find(device_name) == devices.end()) {
        LOG_ERROR(Service_NVDRV, "Trying to open unknown device {}", device_name);
        return INVALID_NVDRV_FD;
    }

    auto device = devices[device_name];
    const DeviceFD fd = next_fd++;

    device->OnOpen(fd, gpu);

    open_files[fd] = std::move(device);

    return fd;
}

NvResult Module::Ioctl1(DeviceFD fd, Ioctl command, const std::vector<u8>& input,
                        std::vector<u8>& output, Shared<Tegra::GPU>& gpu) const {
    if (fd < 0) {
        LOG_ERROR(Service_NVDRV, "Invalid DeviceFD={}!", fd);
        return NvResult::InvalidState;
    }

    const auto itr = open_files.find(fd);

    if (itr == open_files.end()) {
        LOG_ERROR(Service_NVDRV, "Could not find DeviceFD={}!", fd);
        return NvResult::NotImplemented;
    }

    return itr->second->WriteLocked()->Ioctl1(fd, command, input, output, gpu);
}

NvResult Module::Ioctl2(DeviceFD fd, Ioctl command, const std::vector<u8>& input,
                        const std::vector<u8>& inline_input, std::vector<u8>& output,
                        Shared<Tegra::GPU>& gpu) const {
    if (fd < 0) {
        LOG_ERROR(Service_NVDRV, "Invalid DeviceFD={}!", fd);
        return NvResult::InvalidState;
    }

    const auto itr = open_files.find(fd);

    if (itr == open_files.end()) {
        LOG_ERROR(Service_NVDRV, "Could not find DeviceFD={}!", fd);
        return NvResult::NotImplemented;
    }

    return itr->second->WriteLocked()->Ioctl2(fd, command, input, inline_input, output, gpu);
}

NvResult Module::Ioctl3(DeviceFD fd, Ioctl command, const std::vector<u8>& input,
                        std::vector<u8>& output, std::vector<u8>& inline_output,
                        Shared<Tegra::GPU>& gpu) const {
    if (fd < 0) {
        LOG_ERROR(Service_NVDRV, "Invalid DeviceFD={}!", fd);
        return NvResult::InvalidState;
    }

    const auto itr = open_files.find(fd);

    if (itr == open_files.end()) {
        LOG_ERROR(Service_NVDRV, "Could not find DeviceFD={}!", fd);
        return NvResult::NotImplemented;
    }

    return itr->second->WriteLocked()->Ioctl3(fd, command, input, output, inline_output, gpu);
}

NvResult Module::Close(DeviceFD fd, Shared<Tegra::GPU>& gpu) {
    if (fd < 0) {
        LOG_ERROR(Service_NVDRV, "Invalid DeviceFD={}!", fd);
        return NvResult::InvalidState;
    }

    const auto itr = open_files.find(fd);

    if (itr == open_files.end()) {
        LOG_ERROR(Service_NVDRV, "Could not find DeviceFD={}!", fd);
        return NvResult::NotImplemented;
    }

    itr->second->WriteLocked()->OnClose(fd, gpu);

    open_files.erase(itr);

    return NvResult::Success;
}

void Module::SignalSyncpt(const u32 syncpoint_id, const u32 value) {
    SharedWriter ei_locked(events_interface);
    for (u32 i = 0; i < MaxNvEvents; i++) {
        if (ei_locked->assigned_syncpt[i] == syncpoint_id &&
            ei_locked->assigned_value[i] == value) {
            ei_locked->LiberateEvent(i);
            KernelHelpers::SignalEvent(ei_locked->events[i].event);
        }
    }
}

int Module::GetEvent(const u32 event_id) {
    return SharedReader(events_interface)->events[event_id].event;
}

} // namespace Service::Nvidia
