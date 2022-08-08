// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
//
// Adapted by Kent Hall for mizu on Horizon Linux.

#pragma once

#include <sys/mman.h>
#include "core/hle/service/service.h"

namespace Service::HID {

class IRS final : public ServiceFramework<IRS> {
public:
    explicit IRS();
    ~IRS() override;

private:
    void ActivateIrsensor(Kernel::HLERequestContext& ctx);
    void DeactivateIrsensor(Kernel::HLERequestContext& ctx);
    void GetIrsensorSharedMemoryHandle(Kernel::HLERequestContext& ctx);
    void StopImageProcessor(Kernel::HLERequestContext& ctx);
    void RunMomentProcessor(Kernel::HLERequestContext& ctx);
    void RunClusteringProcessor(Kernel::HLERequestContext& ctx);
    void RunImageTransferProcessor(Kernel::HLERequestContext& ctx);
    void GetImageTransferProcessorState(Kernel::HLERequestContext& ctx);
    void RunTeraPluginProcessor(Kernel::HLERequestContext& ctx);
    void GetNpadIrCameraHandle(Kernel::HLERequestContext& ctx);
    void RunPointingProcessor(Kernel::HLERequestContext& ctx);
    void SuspendImageProcessor(Kernel::HLERequestContext& ctx);
    void CheckFirmwareVersion(Kernel::HLERequestContext& ctx);
    void SetFunctionLevel(Kernel::HLERequestContext& ctx);
    void RunImageTransferExProcessor(Kernel::HLERequestContext& ctx);
    void RunIrLedProcessor(Kernel::HLERequestContext& ctx);
    void StopImageProcessorAsync(Kernel::HLERequestContext& ctx);
    void ActivateIrsensorWithFunctionLevel(Kernel::HLERequestContext& ctx);

    const u32 device_handle{0xABCD};

    static constexpr std::size_t shared_mem_size{0x8000};
    static constexpr auto shared_mem_deleter = std::bind(::munmap, std::placeholders::_1, shared_mem_size);
    int shared_mem_fd;
    std::unique_ptr<u8, decltype(shared_mem_deleter)> shared_mem;
};

class IRS_SYS final : public ServiceFramework<IRS_SYS> {
public:
    explicit IRS_SYS();
    ~IRS_SYS() override;
};

} // namespace Service::HID
