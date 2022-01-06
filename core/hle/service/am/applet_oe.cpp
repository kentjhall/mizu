// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/logging/log.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/service/am/am.h"
#include "core/hle/service/am/applet_oe.h"
#include "core/hle/service/nvflinger/nvflinger.h"

namespace Service::AM {

class IApplicationProxy final : public ServiceFramework<IApplicationProxy> {
public:
    explicit IApplicationProxy(std::shared_ptr<Shared<AppletMessageQueue>> msg_queue_)
        : ServiceFramework{"IApplicationProxy"}, msg_queue{std::move(msg_queue_)} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, &IApplicationProxy::GetCommonStateGetter, "GetCommonStateGetter"},
            {1, &IApplicationProxy::GetSelfController, "GetSelfController"},
            {2, &IApplicationProxy::GetWindowController, "GetWindowController"},
            {3, &IApplicationProxy::GetAudioController, "GetAudioController"},
            {4, &IApplicationProxy::GetDisplayController, "GetDisplayController"},
            {10, nullptr, "GetProcessWindingController"},
            {11, &IApplicationProxy::GetLibraryAppletCreator, "GetLibraryAppletCreator"},
            {20, &IApplicationProxy::GetApplicationFunctions, "GetApplicationFunctions"},
            {1000, &IApplicationProxy::GetDebugFunctions, "GetDebugFunctions"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }

private:
    void GetAudioController(Kernel::HLERequestContext& ctx) {
        LOG_DEBUG(Service_AM, "called");

        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(ResultSuccess);
        rb.PushIpcInterface<IAudioController>();
    }

    void GetDisplayController(Kernel::HLERequestContext& ctx) {
        LOG_DEBUG(Service_AM, "called");

        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(ResultSuccess);
        rb.PushIpcInterface<IDisplayController>();
    }

    void GetDebugFunctions(Kernel::HLERequestContext& ctx) {
        LOG_DEBUG(Service_AM, "called");

        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(ResultSuccess);
        rb.PushIpcInterface<IDebugFunctions>();
    }

    void GetWindowController(Kernel::HLERequestContext& ctx) {
        LOG_DEBUG(Service_AM, "called");

        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(ResultSuccess);
        rb.PushIpcInterface<IWindowController>();
    }

    void GetSelfController(Kernel::HLERequestContext& ctx) {
        LOG_DEBUG(Service_AM, "called");

        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(ResultSuccess);
        rb.PushIpcInterface<ISelfController>();
    }

    void GetCommonStateGetter(Kernel::HLERequestContext& ctx) {
        LOG_DEBUG(Service_AM, "called");

        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(ResultSuccess);
        rb.PushIpcInterface<ICommonStateGetter>(msg_queue);
    }

    void GetLibraryAppletCreator(Kernel::HLERequestContext& ctx) {
        LOG_DEBUG(Service_AM, "called");

        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(ResultSuccess);
        rb.PushIpcInterface<ILibraryAppletCreator>();
    }

    void GetApplicationFunctions(Kernel::HLERequestContext& ctx) {
        LOG_DEBUG(Service_AM, "called");

        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(ResultSuccess);
        rb.PushIpcInterface<IApplicationFunctions>();
    }

    std::shared_ptr<Shared<AppletMessageQueue>> msg_queue;
};

void AppletOE::OpenApplicationProxy(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<IApplicationProxy>(msg_queue);
}

AppletOE::AppletOE(std::shared_ptr<Shared<AppletMessageQueue>> msg_queue_)
    : ServiceFramework{"appletOE"}, msg_queue{std::move(msg_queue_)} {
    static const FunctionInfo functions[] = {
        {0, &AppletOE::OpenApplicationProxy, "OpenApplicationProxy"},
    };
    RegisterHandlers(functions);
}

AppletOE::~AppletOE() = default;

const std::shared_ptr<Shared<AppletMessageQueue>>& AppletOE::GetMessageQueue() const {
    return msg_queue;
}

} // namespace Service::AM