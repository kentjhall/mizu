// Copyright 2021 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/core.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/k_event.h"
#include "core/hle/service/acc/async_context.h"

namespace Service::Account {
IAsyncContext::IAsyncContext()
    : ServiceFramework{"IAsyncContext"} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, &IAsyncContext::GetSystemEvent, "GetSystemEvent"},
        {1, &IAsyncContext::Cancel, "Cancel"},
        {2, &IAsyncContext::HasDone, "HasDone"},
        {3, &IAsyncContext::GetResult, "GetResult"},
    };
    // clang-format on

    RegisterHandlers(functions);

    KernelHelpers::SetupServiceContext("IAsyncContext");
    completion_event = KernelHelpers::CreateEvent("IAsyncContext:CompletionEvent");
}

IAsyncContext::~IAsyncContext() {
    KernelHelpers::CloseEvent(completion_event);
}

void IAsyncContext::GetSystemEvent(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_ACC, "called");

    IPC::ResponseBuilder rb{ctx, 2, 1};
    rb.Push(ResultSuccess);
    rb.PushCopyFds(completion_event);
}

void IAsyncContext::Cancel(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_ACC, "called");

    Cancel();
    MarkComplete();

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IAsyncContext::HasDone(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_ACC, "called");

    is_complete.store(IsComplete());

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(is_complete.load());
}

void IAsyncContext::GetResult(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_ACC, "called");

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(GetResult());
}

void IAsyncContext::MarkComplete() {
    is_complete.store(true);
    KernelHelpers::SignalEvent(completion_event);
}

} // namespace Service::Account
