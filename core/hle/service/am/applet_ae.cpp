// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/logging/log.h"
#include "core/core.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/service/am/am.h"
#include "core/hle/service/am/applet_ae.h"
#include "core/hle/service/nvflinger/nvflinger.h"

namespace Service::AM {

class ILibraryAppletProxy final : public ServiceFramework<ILibraryAppletProxy> {
public:
    explicit ILibraryAppletProxy(std::shared_ptr<Shared<AppletMessageQueue>> msg_queue_)
        : ServiceFramework{"ILibraryAppletProxy"}, msg_queue{std::move(msg_queue_)} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, &ILibraryAppletProxy::GetCommonStateGetter, "GetCommonStateGetter"},
            {1, &ILibraryAppletProxy::GetSelfController, "GetSelfController"},
            {2, &ILibraryAppletProxy::GetWindowController, "GetWindowController"},
            {3, &ILibraryAppletProxy::GetAudioController, "GetAudioController"},
            {4, &ILibraryAppletProxy::GetDisplayController, "GetDisplayController"},
            {10, &ILibraryAppletProxy::GetProcessWindingController, "GetProcessWindingController"},
            {11, &ILibraryAppletProxy::GetLibraryAppletCreator, "GetLibraryAppletCreator"},
            {20, &ILibraryAppletProxy::GetApplicationFunctions, "GetApplicationFunctions"},
            {21, nullptr, "GetAppletCommonFunctions"},
            {1000, &ILibraryAppletProxy::GetDebugFunctions, "GetDebugFunctions"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }

private:
    void GetCommonStateGetter(Kernel::HLERequestContext& ctx) {
        LOG_DEBUG(Service_AM, "called");

        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(ResultSuccess);
        rb.PushIpcInterface<ICommonStateGetter>(msg_queue);
    }

    void GetSelfController(Kernel::HLERequestContext& ctx) {
        LOG_DEBUG(Service_AM, "called");

        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(ResultSuccess);
        rb.PushIpcInterface<ISelfController>();
    }

    void GetWindowController(Kernel::HLERequestContext& ctx) {
        LOG_DEBUG(Service_AM, "called");

        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(ResultSuccess);
        rb.PushIpcInterface<IWindowController>();
    }

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

    void GetProcessWindingController(Kernel::HLERequestContext& ctx) {
        LOG_DEBUG(Service_AM, "called");

        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(ResultSuccess);
        rb.PushIpcInterface<IProcessWindingController>();
    }

    void GetDebugFunctions(Kernel::HLERequestContext& ctx) {
        LOG_DEBUG(Service_AM, "called");

        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(ResultSuccess);
        rb.PushIpcInterface<IDebugFunctions>();
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

class ISystemAppletProxy final : public ServiceFramework<ISystemAppletProxy> {
public:
    explicit ISystemAppletProxy(std::shared_ptr<Shared<AppletMessageQueue>> msg_queue_)
        : ServiceFramework{"ISystemAppletProxy"}, msg_queue{std::move(msg_queue_)} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, &ISystemAppletProxy::GetCommonStateGetter, "GetCommonStateGetter"},
            {1, &ISystemAppletProxy::GetSelfController, "GetSelfController"},
            {2, &ISystemAppletProxy::GetWindowController, "GetWindowController"},
            {3, &ISystemAppletProxy::GetAudioController, "GetAudioController"},
            {4, &ISystemAppletProxy::GetDisplayController, "GetDisplayController"},
            {10, nullptr, "GetProcessWindingController"},
            {11, &ISystemAppletProxy::GetLibraryAppletCreator, "GetLibraryAppletCreator"},
            {20, &ISystemAppletProxy::GetHomeMenuFunctions, "GetHomeMenuFunctions"},
            {21, &ISystemAppletProxy::GetGlobalStateController, "GetGlobalStateController"},
            {22, &ISystemAppletProxy::GetApplicationCreator, "GetApplicationCreator"},
            {23, nullptr, "GetAppletCommonFunctions"},
            {1000, &ISystemAppletProxy::GetDebugFunctions, "GetDebugFunctions"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }

private:
    void GetCommonStateGetter(Kernel::HLERequestContext& ctx) {
        LOG_DEBUG(Service_AM, "called");

        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(ResultSuccess);
        rb.PushIpcInterface<ICommonStateGetter>(msg_queue);
    }

    void GetSelfController(Kernel::HLERequestContext& ctx) {
        LOG_DEBUG(Service_AM, "called");

        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(ResultSuccess);
        rb.PushIpcInterface<ISelfController>();
    }

    void GetWindowController(Kernel::HLERequestContext& ctx) {
        LOG_DEBUG(Service_AM, "called");

        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(ResultSuccess);
        rb.PushIpcInterface<IWindowController>();
    }

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

    void GetLibraryAppletCreator(Kernel::HLERequestContext& ctx) {
        LOG_DEBUG(Service_AM, "called");

        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(ResultSuccess);
        rb.PushIpcInterface<ILibraryAppletCreator>();
    }

    void GetHomeMenuFunctions(Kernel::HLERequestContext& ctx) {
        LOG_DEBUG(Service_AM, "called");

        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(ResultSuccess);
        rb.PushIpcInterface<IHomeMenuFunctions>();
    }

    void GetGlobalStateController(Kernel::HLERequestContext& ctx) {
        LOG_DEBUG(Service_AM, "called");

        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(ResultSuccess);
        rb.PushIpcInterface<IGlobalStateController>();
    }

    void GetApplicationCreator(Kernel::HLERequestContext& ctx) {
        LOG_DEBUG(Service_AM, "called");

        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(ResultSuccess);
        rb.PushIpcInterface<IApplicationCreator>();
    }

    std::shared_ptr<Shared<AppletMessageQueue>> msg_queue;
};

void AppletAE::OpenSystemAppletProxy(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<ISystemAppletProxy>(msg_queue);
}

void AppletAE::OpenLibraryAppletProxy(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<ILibraryAppletProxy>(msg_queue);
}

void AppletAE::OpenLibraryAppletProxyOld(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<ILibraryAppletProxy>(msg_queue);
}

AppletAE::AppletAE(std::shared_ptr<Shared<AppletMessageQueue>> msg_queue_)
    : ServiceFramework{"appletAE"}, msg_queue{std::move(msg_queue_)} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {100, &AppletAE::OpenSystemAppletProxy, "OpenSystemAppletProxy"},
        {200, &AppletAE::OpenLibraryAppletProxyOld, "OpenLibraryAppletProxyOld"},
        {201, &AppletAE::OpenLibraryAppletProxy, "OpenLibraryAppletProxy"},
        {300, nullptr, "OpenOverlayAppletProxy"},
        {350, nullptr, "OpenSystemApplicationProxy"},
        {400, nullptr, "CreateSelfLibraryAppletCreatorForDevelop"},
        {410, nullptr, "GetSystemAppletControllerForDebug"},
        {1000, nullptr, "GetDebugFunctions"},
    };
    // clang-format on

    RegisterHandlers(functions);
}

AppletAE::~AppletAE() = default;

const std::shared_ptr<Shared<AppletMessageQueue>>& AppletAE::GetMessageQueue() const {
    return msg_queue;
}

} // namespace Service::AM
