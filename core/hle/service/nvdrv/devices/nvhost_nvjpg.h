// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <vector>
#include "common/common_types.h"
#include "common/swap.h"
#include "core/hle/service/nvdrv/devices/nvdevice.h"

namespace Service::Nvidia::Devices {

class nvhost_nvjpg final : public nvdevice {
public:
    explicit nvhost_nvjpg();
    ~nvhost_nvjpg() override;

    NvResult Ioctl1(DeviceFD fd, Ioctl command, const std::vector<u8>& input,
                    std::vector<u8>& output, Shared<Tegra::GPU>& gpu) override;
    NvResult Ioctl2(DeviceFD fd, Ioctl command, const std::vector<u8>& input,
                    const std::vector<u8>& inline_input, std::vector<u8>& output, Shared<Tegra::GPU>& gpu) override;
    NvResult Ioctl3(DeviceFD fd, Ioctl command, const std::vector<u8>& input,
                    std::vector<u8>& output, std::vector<u8>& inline_output, Shared<Tegra::GPU>& gpu) override;

    void OnOpen(DeviceFD fd, Shared<Tegra::GPU>& gpu) override;
    void OnClose(DeviceFD fd, Shared<Tegra::GPU>& gpu) override;

private:
    struct IoctlSetNvmapFD {
        s32_le nvmap_fd{};
    };
    static_assert(sizeof(IoctlSetNvmapFD) == 4, "IoctlSetNvmapFD is incorrect size");

    s32_le nvmap_fd{};

    NvResult SetNVMAPfd(const std::vector<u8>& input, std::vector<u8>& output);
};

} // namespace Service::Nvidia::Devices
