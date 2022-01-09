// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <vector>
#include "common/bit_field.h"
#include "common/common_types.h"
#include "common/swap.h"
#include "core/hle/service/nvdrv/nvdata.h"
#include "core/hle/service/service.h"

namespace Service::Nvidia::Devices {

/// Represents an abstract nvidia device node. It is to be subclassed by concrete device nodes to
/// implement the ioctl interface.
class nvdevice {
public:
    explicit nvdevice() {}
    virtual ~nvdevice() = default;

    /**
     * Handles an ioctl1 request.
     * @param command The ioctl command id.
     * @param input A buffer containing the input data for the ioctl.
     * @param output A buffer where the output data will be written to.
     * @param GPU for this session.
     * @returns The result code of the ioctl.
     */
    virtual NvResult Ioctl1(DeviceFD fd, Ioctl command, const std::vector<u8>& input,
                            std::vector<u8>& output, Shared<Tegra::GPU>& gpu) = 0;

    /**
     * Handles an ioctl2 request.
     * @param command The ioctl command id.
     * @param input A buffer containing the input data for the ioctl.
     * @param inline_input A buffer containing the input data for the ioctl which has been inlined.
     * @param output A buffer where the output data will be written to.
     * @returns The result code of the ioctl.
     */
    virtual NvResult Ioctl2(DeviceFD fd, Ioctl command, const std::vector<u8>& input,
                            const std::vector<u8>& inline_input, std::vector<u8>& output,
			    Shared<Tegra::GPU>& gpu) = 0;

    /**
     * Handles an ioctl3 request.
     * @param command The ioctl command id.
     * @param input A buffer containing the input data for the ioctl.
     * @param output A buffer where the output data will be written to.
     * @param inline_output A buffer where the inlined output data will be written to.
     * @returns The result code of the ioctl.
     */
    virtual NvResult Ioctl3(DeviceFD fd, Ioctl command, const std::vector<u8>& input,
                            std::vector<u8>& output, std::vector<u8>& inline_output,
			    Shared<Tegra::GPU>& gpu) = 0;

    /**
     * Called once a device is openned
     * @param fd The device fd
     */
    virtual void OnOpen(DeviceFD fd, Shared<Tegra::GPU>& gpu) = 0;

    /**
     * Called once a device is closed
     * @param fd The device fd
     */
    virtual void OnClose(DeviceFD fd, Shared<Tegra::GPU>& gpu) = 0;

    auto ReadLocked() { return Service::SharedReader(mtx, *this); };
    auto WriteLocked() { return Service::SharedWriter(mtx, *this); };

protected:
    std::shared_mutex mtx;
};

template <class Derived>
class nvdevice_locked : public nvdevice {
public:
    explicit nvdevice_locked() : nvdevice() {}

    auto ReadLocked() { return Service::SharedReader(mtx, *static_cast<const Derived *>(this)); };
    auto WriteLocked() { return Service::SharedWriter(mtx, *static_cast<Derived *>(this)); };
};

} // namespace Service::Nvidia::Devices
