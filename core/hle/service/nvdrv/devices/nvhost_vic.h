// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
//
// Adapted by Kent Hall for mizu on Horizon Linux.

#pragma once

#include "core/hle/service/nvdrv/devices/nvhost_nvdec_common.h"

namespace Service::Nvidia::Devices {

class nvhost_vic final : public nvhost_nvdec_common {
public:
    explicit nvhost_vic(std::shared_ptr<nvmap> nvmap_dev_,
                        SyncpointManager& syncpoint_manager_);
    ~nvhost_vic();

    NvResult Ioctl1(DeviceFD fd, Ioctl command, const std::vector<u8>& input,
                    std::vector<u8>& output, Shared<Tegra::GPU>& gpu) override;
    NvResult Ioctl2(DeviceFD fd, Ioctl command, const std::vector<u8>& input,
                    const std::vector<u8>& inline_input, std::vector<u8>& output, Shared<Tegra::GPU>& gpu) override;
    NvResult Ioctl3(DeviceFD fd, Ioctl command, const std::vector<u8>& input,
                    std::vector<u8>& output, std::vector<u8>& inline_output, Shared<Tegra::GPU>& gpu) override;

    void OnOpen(DeviceFD fd, Shared<Tegra::GPU>& gpu) override;
    void OnClose(DeviceFD fd, Shared<Tegra::GPU>& gpu) override;
};
} // namespace Service::Nvidia::Devices
