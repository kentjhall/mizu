// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
//
// Adapted by Kent Hall for mizu on Horizon Linux.

#pragma once

#include <memory>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <optional>
#include <sys/types.h>

#include "common/concepts.h"
#include "core/hle/result.h"
#include "core/hle/service/service.h"
#include "core/hle/service/sm/sm_controller.h"

namespace Service::SM {

class Controller;

/// Interface to "sm:" service
class SM final : public ServiceFramework<SM> {
public:
    explicit SM();

    void SetupSession(::pid_t req_pid) override;
    void CleanupSession(::pid_t req_pid) override;

private:
    void Initialize(Kernel::HLERequestContext& ctx);
    void GetService(Kernel::HLERequestContext& ctx);
    void GetServiceTipc(Kernel::HLERequestContext& ctx);
    void RegisterService(Kernel::HLERequestContext& ctx);
    void UnregisterService(Kernel::HLERequestContext& ctx);

    ResultVal<Kernel::Handle> GetServiceImpl(Kernel::HLERequestContext& ctx);

    bool is_initialized{};
};

class ServiceManager {
public:
    explicit ServiceManager();
    ~ServiceManager();

    ResultCode RegisterService(std::shared_ptr<Service::ServiceFrameworkBase> handler,
                               ::pid_t handler_pid);
    ResultCode RegisterService(std::string name, u32 max_sessions,
                               Kernel::SessionRequestHandlerPtr handler, ::pid_t handler_pid);
    ResultCode UnregisterService(const std::string& name);
    ResultVal<Kernel::Handle> GetServicePort(const std::string& name) const;

    template <Common::DerivedFrom<Kernel::SessionRequestHandler> T>
    std::shared_ptr<T> GetService(const std::string& service_name) const {
        auto service = registered_services.find(service_name);
        if (service == registered_services.end()) {
            LOG_DEBUG(Service, "Can't find service: {}", service_name);
            return nullptr;
        }
        return std::static_pointer_cast<T>(service->second.first);
    }

    void InvokeControlRequest(Kernel::HLERequestContext& context);

private:
    std::shared_ptr<SM> sm_interface;
    Controller controller_interface;

    /// Map of registered services, retrieved using GetServicePort.
    std::unordered_map<std::string,
                       std::pair<Kernel::SessionRequestHandlerPtr, ::pid_t>> registered_services;
};

} // namespace Service::SM
