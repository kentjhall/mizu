// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
//
// Adapted by Kent Hall for mizu on Horizon Linux.

#include "common/assert.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/service/sm/sm_controller.h"
#include "horizon_servctl.h"

namespace Service::SM {

void Controller::ConvertCurrentObjectToDomain(Kernel::HLERequestContext& ctx) {
    ASSERT_MSG(!ctx.IsDomain(), "Session is already a domain");
    LOG_DEBUG(Service, "called, server_session={}", ctx.GetSessionId());
    ctx.ConvertToDomain();

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push<u32>(1); // Converted sessions start with 1 request handler
}

void Controller::CloneCurrentObject(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service, "called");

    // Create a session.
    int session_handle = horizon_servctl(HZN_SCTL_CREATE_SESSION_HANDLE, 0,
                                      AddSessionManager(ctx.GetSessionRequestManagerShared()));
    if (session_handle < 0) {
        ResultCode result(errno);
        LOG_CRITICAL(Service, "HZN_SCTL_CREATE_SESSION_HANDLE failed with error 0x{:08X}", result.raw);
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(result);
    }

    // We succeeded.
    IPC::ResponseBuilder rb{ctx, 2, 0, 1, IPC::ResponseBuilder::Flags::AlwaysMoveHandles};
    rb.Push(ResultSuccess);
    rb.PushMoveHandles(Kernel::Handle(session_handle));
}

void Controller::CloneCurrentObjectEx(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service, "called");

    CloneCurrentObject(ctx);
}

void Controller::QueryPointerBufferSize(Kernel::HLERequestContext& ctx) {
    LOG_WARNING(Service, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push<u16>(0x8000);
}

// https://switchbrew.org/wiki/IPC_Marshalling
Controller::Controller() : ServiceFramework{"IpcController"} {
    static const FunctionInfo functions[] = {
        {0, &Controller::ConvertCurrentObjectToDomain, "ConvertCurrentObjectToDomain"},
        {1, nullptr, "CopyFromCurrentDomain"},
        {2, &Controller::CloneCurrentObject, "CloneCurrentObject"},
        {3, &Controller::QueryPointerBufferSize, "QueryPointerBufferSize"},
        {4, &Controller::CloneCurrentObjectEx, "CloneCurrentObjectEx"},
    };
    RegisterHandlers(functions);
}

Controller::~Controller() = default;

} // namespace Service::SM
