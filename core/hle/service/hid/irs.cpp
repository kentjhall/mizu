// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <ctime>
#include <sys/mman.h>
#include "common/swap.h"
#include "core/core.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/service/hid/irs.h"

namespace Service::HID {

IRS::IRS() : ServiceFramework{"irs"}, shared_mem{nullptr, shared_mem_deleter} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {302, &IRS::ActivateIrsensor, "ActivateIrsensor"},
        {303, &IRS::DeactivateIrsensor, "DeactivateIrsensor"},
        {304, &IRS::GetIrsensorSharedMemoryHandle, "GetIrsensorSharedMemoryHandle"},
        {305, &IRS::StopImageProcessor, "StopImageProcessor"},
        {306, &IRS::RunMomentProcessor, "RunMomentProcessor"},
        {307, &IRS::RunClusteringProcessor, "RunClusteringProcessor"},
        {308, &IRS::RunImageTransferProcessor, "RunImageTransferProcessor"},
        {309, &IRS::GetImageTransferProcessorState, "GetImageTransferProcessorState"},
        {310, &IRS::RunTeraPluginProcessor, "RunTeraPluginProcessor"},
        {311, &IRS::GetNpadIrCameraHandle, "GetNpadIrCameraHandle"},
        {312, &IRS::RunPointingProcessor, "RunPointingProcessor"},
        {313, &IRS::SuspendImageProcessor, "SuspendImageProcessor"},
        {314, &IRS::CheckFirmwareVersion, "CheckFirmwareVersion"},
        {315, &IRS::SetFunctionLevel, "SetFunctionLevel"},
        {316, &IRS::RunImageTransferExProcessor, "RunImageTransferExProcessor"},
        {317, &IRS::RunIrLedProcessor, "RunIrLedProcessor"},
        {318, &IRS::StopImageProcessorAsync, "StopImageProcessorAsync"},
        {319, &IRS::ActivateIrsensorWithFunctionLevel, "ActivateIrsensorWithFunctionLevel"},
    };
    // clang-format on

    RegisterHandlers(functions);

    shared_mem_fd = ::memfd_create("mizu_irs", 0);
    if (shared_mem_fd == -1) {
        LOG_CRITICAL(Service_HID, "memfd_create failed: {}", ::strerror(errno));
    } else {
        u8 *shared_mapping = static_cast<u8 *>(::mmap(NULL, shared_mem_size, PROT_READ | PROT_WRITE,
                                                      MAP_SHARED, shared_mem_fd, 0));
        if (shared_mapping == MAP_FAILED) {
            LOG_CRITICAL(Service_HID, "mmap failed: {}", ::strerror(errno));
        } else {
            shared_mem.reset(shared_mapping);
        }
    }
}

void IRS::ActivateIrsensor(Kernel::HLERequestContext& ctx) {
    LOG_WARNING(Service_IRS, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IRS::DeactivateIrsensor(Kernel::HLERequestContext& ctx) {
    LOG_WARNING(Service_IRS, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IRS::GetIrsensorSharedMemoryHandle(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_IRS, "called");

    IPC::ResponseBuilder rb{ctx, 2, 1};
    rb.Push(ResultSuccess);
    rb.PushCopyFds(shared_mem_fd);
}

void IRS::StopImageProcessor(Kernel::HLERequestContext& ctx) {
    LOG_WARNING(Service_IRS, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IRS::RunMomentProcessor(Kernel::HLERequestContext& ctx) {
    LOG_WARNING(Service_IRS, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IRS::RunClusteringProcessor(Kernel::HLERequestContext& ctx) {
    LOG_WARNING(Service_IRS, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IRS::RunImageTransferProcessor(Kernel::HLERequestContext& ctx) {
    LOG_WARNING(Service_IRS, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IRS::GetImageTransferProcessorState(Kernel::HLERequestContext& ctx) {
    LOG_WARNING(Service_IRS, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 5};
    rb.Push(ResultSuccess);
    rb.PushRaw<u64>(static_cast<u64>(::clock()));
    rb.PushRaw<u32>(0);
}

void IRS::RunTeraPluginProcessor(Kernel::HLERequestContext& ctx) {
    LOG_WARNING(Service_IRS, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IRS::GetNpadIrCameraHandle(Kernel::HLERequestContext& ctx) {
    LOG_WARNING(Service_IRS, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.PushRaw<u32>(device_handle);
}

void IRS::RunPointingProcessor(Kernel::HLERequestContext& ctx) {
    LOG_WARNING(Service_IRS, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IRS::SuspendImageProcessor(Kernel::HLERequestContext& ctx) {
    LOG_WARNING(Service_IRS, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IRS::CheckFirmwareVersion(Kernel::HLERequestContext& ctx) {
    LOG_WARNING(Service_IRS, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IRS::SetFunctionLevel(Kernel::HLERequestContext& ctx) {
    LOG_WARNING(Service_IRS, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IRS::RunImageTransferExProcessor(Kernel::HLERequestContext& ctx) {
    LOG_WARNING(Service_IRS, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IRS::RunIrLedProcessor(Kernel::HLERequestContext& ctx) {
    LOG_WARNING(Service_IRS, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IRS::StopImageProcessorAsync(Kernel::HLERequestContext& ctx) {
    LOG_WARNING(Service_IRS, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IRS::ActivateIrsensorWithFunctionLevel(Kernel::HLERequestContext& ctx) {
    LOG_WARNING(Service_IRS, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

IRS::~IRS() = default;

IRS_SYS::IRS_SYS() : ServiceFramework{"irs:sys"} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {500, nullptr, "SetAppletResourceUserId"},
        {501, nullptr, "RegisterAppletResourceUserId"},
        {502, nullptr, "UnregisterAppletResourceUserId"},
        {503, nullptr, "EnableAppletToGetInput"},
    };
    // clang-format on

    RegisterHandlers(functions);
}

IRS_SYS::~IRS_SYS() = default;

} // namespace Service::HID
