// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <tuple>
#include "common/assert.h"
#include "common/scope_exit.h"
#include "configuration/config.h"
#include "core/core.h"
#include "core/hle/kernel/hle_ipc.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/result.h"
#include "core/hle/service/service.h"
#include "core/hle/service/nvflinger/nvflinger.h"
#include "core/hle/service/sm/sm.h"
#include "core/hle/service/sm/sm_controller.h"
#include "horizon_servctl.h"

namespace Service::SM {

constexpr ResultCode ERR_NOT_INITIALIZED(ErrorModule::SM, 2);
constexpr ResultCode ERR_ALREADY_REGISTERED(ErrorModule::SM, 4);
constexpr ResultCode ERR_INVALID_NAME(ErrorModule::SM, 6);
constexpr ResultCode ERR_SERVICE_NOT_REGISTERED(ErrorModule::SM, 7);

ServiceManager::ServiceManager() {}
ServiceManager::~ServiceManager() = default;

void ServiceManager::InvokeControlRequest(Kernel::HLERequestContext& context) {
    controller_interface.InvokeRequest(context);
}

static ResultCode ValidateServiceName(const std::string& name) {
    if (name.empty() || name.size() > 8) {
        LOG_ERROR(Service_SM, "Invalid service name! service={}", name);
        return ERR_INVALID_NAME;
    }
    return ResultSuccess;
}

ResultCode ServiceManager::RegisterService(std::string name, u32 max_sessions,
                                           Kernel::SessionRequestHandlerPtr handler, ::pid_t handler_pid) {

    CASCADE_CODE(ValidateServiceName(name));

    if (registered_services.find(name) != registered_services.end()) {
        LOG_ERROR(Service_SM, "Service is already registered! service={}", name);
        return ERR_ALREADY_REGISTERED;
    }

    registered_services.emplace(std::move(name),
                                std::make_pair(std::move(handler), handler_pid));

    return ResultSuccess;
}

ResultCode ServiceManager::RegisterService(std::shared_ptr<Service::ServiceFrameworkBase> handler,
                                           ::pid_t handler_pid) {
    return RegisterService(handler->GetServiceName(), handler->GetMaxSessions(), handler, handler_pid);
}

ResultCode ServiceManager::UnregisterService(const std::string& name) {
    CASCADE_CODE(ValidateServiceName(name));

    const auto iter = registered_services.find(name);
    if (iter == registered_services.end()) {
        LOG_ERROR(Service_SM, "Server is not registered! service={}", name);
        return ERR_SERVICE_NOT_REGISTERED;
    }

    registered_services.erase(iter);
    return ResultSuccess;
}

ResultVal<Kernel::Handle> ServiceManager::GetServicePort(const std::string& name) const {
    CASCADE_CODE(ValidateServiceName(name));
    auto it = registered_services.find(name);
    if (it == registered_services.end()) {
        LOG_ERROR(Service_SM, "Server is not registered! service={}", name);
        return ERR_SERVICE_NOT_REGISTERED;
    }

    int port = horizon_servctl(HZN_SCTL_CREATE_SESSION_HANDLE, it->second.second, 0);
    if (port == -1) {
        return ResultCode(errno);
    }

    return MakeResult(Kernel::Handle(port));
}

void SM::SetupSession(::pid_t req_pid) {
    Config::config->Reread();
}

void SM::CleanupSession(::pid_t req_pid) {
    SharedUnlocked(nv_flinger)->CloseSessionLayers(req_pid);
}

/**
 * SM::Initialize service function
 *  Inputs:
 *      0: 0x00000000
 *  Outputs:
 *      0: ResultCode
 */
void SM::Initialize(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_SM, "called");

    is_initialized = true;

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void SM::GetService(Kernel::HLERequestContext& ctx) {
    auto result = GetServiceImpl(ctx);
    if (result.Succeeded()) {
        IPC::ResponseBuilder rb{ctx, 2, 0, 1, IPC::ResponseBuilder::Flags::AlwaysMoveHandles};
        rb.Push(result.Code());
        rb.PushMoveHandles(result.Unwrap());
    } else {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(result.Code());
    }
}

void SM::GetServiceTipc(Kernel::HLERequestContext& ctx) {
    auto result = GetServiceImpl(ctx);
    IPC::ResponseBuilder rb{ctx, 2, 0, 1, IPC::ResponseBuilder::Flags::AlwaysMoveHandles};
    rb.Push(result.Code());
    rb.PushMoveHandles(result.Succeeded() ? result.Unwrap() : Kernel::Svc::InvalidHandle);
}

static std::string PopServiceName(IPC::RequestParser& rp) {
    auto name_buf = rp.PopRaw<std::array<char, 8>>();
    std::string result;
    for (const auto& c : name_buf) {
        if (c >= ' ' && c <= '~') {
            result.push_back(c);
        }
    }
    return result;
}

ResultVal<Kernel::Handle> SM::GetServiceImpl(Kernel::HLERequestContext& ctx) {
    if (!is_initialized) {
        return ERR_NOT_INITIALIZED;
    }

    IPC::RequestParser rp{ctx};
    std::string name(PopServiceName(rp));

    // Find the named port.
    auto port_result = SharedReader(service_manager)->GetServicePort(name);
    if (port_result.Failed()) {
        LOG_ERROR(Service_SM, "called service={} -> error 0x{:08X}", name, port_result.Code().raw);
        return port_result.Code();
    }
    Kernel::Handle port = port_result.Unwrap();

    LOG_DEBUG(Service_SM, "called service={} -> session={}", name, ctx.GetSessionId());

    return MakeResult(port);
}

void SM::RegisterService(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    std::string name(PopServiceName(rp));

    const auto is_light = static_cast<bool>(rp.PopRaw<u32>());
    const auto max_session_count = rp.PopRaw<u32>();

    LOG_DEBUG(Service_SM, "called with name={}, max_session_count={}, is_light={}", name,
              max_session_count, is_light);

    if (const auto result = SharedWriter(service_manager)->RegisterService(name, max_session_count, nullptr, -1);
        result.IsError()) {
        LOG_ERROR(Service_SM, "failed to register service with error_code={:08X}", result.raw);
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(result);
        return;
    }

    int port = horizon_servctl(HZN_SCTL_CREATE_SESSION_HANDLE, -1, 0);
    if (port == -1) {
        ResultCode result(errno);
        LOG_ERROR(Service_SM, "failed to HZN_SCTL_CREATE_SESSION_HANDLE with error_code={:08X}", result.raw);
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(result);
        return;
    }

    IPC::ResponseBuilder rb{ctx, 2, 0, 1, IPC::ResponseBuilder::Flags::AlwaysMoveHandles};
    rb.Push(ResultSuccess);
    rb.PushMoveHandles(Kernel::Handle(port));
}

void SM::UnregisterService(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    std::string name(PopServiceName(rp));

    LOG_DEBUG(Service_SM, "called with name={}", name);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(SharedWriter(service_manager)->UnregisterService(name));
}

SM::SM()
    : ServiceFramework{"sm:", 4} {
    RegisterHandlers({
        {0, &SM::Initialize, "Initialize"},
        {1, &SM::GetService, "GetService"},
        {2, &SM::RegisterService, "RegisterService"},
        {3, &SM::UnregisterService, "UnregisterService"},
        {4, nullptr, "DetachClient"},
    });
    RegisterHandlersTipc({
        {0, &SM::Initialize, "Initialize"},
        {1, &SM::GetServiceTipc, "GetService"},
        {2, &SM::RegisterService, "RegisterService"},
        {3, &SM::UnregisterService, "UnregisterService"},
        {4, nullptr, "DetachClient"},
    });
}

} // namespace Service::SM
