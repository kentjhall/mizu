// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_set>
#include <thread>
#include <chrono>
#include <boost/container/flat_map.hpp>
#include <unistd.h>
#include <sys/syscall.h>
#include "common/common_types.h"
#include "common/spin_lock.h"
#include "video_core/gpu.h"
#include "video_core/video_core.h"
#include "input_common/main.h"
#include "core/reporter.h"
#include "core/hardware_interrupt_manager.h"
#include "core/perf_stats.h"
#include "core/loader/loader.h"
#include "core/file_sys/registered_cache.h"
#include "core/file_sys/vfs_real.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/svc_results.h"
#include "core/hle/kernel/hle_ipc.h"
#include "mizu_servctl.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace Service

namespace Service {

namespace FileSystem {
class FileSystemController;
}

namespace NVFlinger {
class NVFlinger;
}

namespace SM {
class ServiceManager;
}

namespace APM {
class Controller;
}

namespace AM {
namespace Applets {
class AppletManager;
}
}

namespace Glue {
class ARPManager;
}

template <typename T>
class Shared;

template <typename T>
class SharedReader {
public:
    SharedReader(Shared<T>& shared)
        : l(shared.mtx), data(shared.data) {}
    SharedReader(std::shared_mutex& mtx, const T& data)
        : l(mtx), data(data) {}
    const T *operator->() { return &data; }
    const T& operator*() { return data; }
private:
    std::shared_lock<std::shared_mutex> l;
    const T& data;
};

template <typename T>
class SharedWriter {
public:
    SharedWriter(Shared<T>& shared)
        : l(shared.mtx), data(shared.data) {}
    SharedWriter(std::shared_mutex& mtx, T& data)
        : l(mtx), data(data) {}
    T *operator->() { return &data; }
    T& operator*() { return data; }
private:
    std::unique_lock<std::shared_mutex> l;
    T& data;
};

template <typename T>
class SharedUnlocked {
public:
    SharedUnlocked(Shared<T>& shared)
        : data(shared.data) {}
    T *operator->() { return &data; }
    T& operator*() { return data; }
private:
    T& data;
};

template <typename T>
class Shared {
public:
    Shared(Shared&& shared)
        : data(std::move(shared.data)) {}
    template <typename... Args>
    Shared(Args&&... args)
        : data(std::forward<Args>(args)...) {}
private:
    friend class SharedReader<T>;
    friend class SharedWriter<T>;
    friend class SharedUnlocked<T>;
    std::shared_mutex mtx;
    T data;
};

extern Shared<SM::ServiceManager> service_manager;
extern Shared<FileSystem::FileSystemController> filesystem_controller;
extern Shared<FileSys::ContentProviderUnion> content_provider;
extern Shared<FileSys::RealVfsFilesystem> filesystem;
extern Shared<APM::Controller> apm_controller;
extern Shared<AM::Applets::AppletManager> applet_manager;
extern Shared<NVFlinger::NVFlinger> nv_flinger;
extern Shared<Glue::ARPManager> arp_manager;
extern Shared<InputCommon::InputSubsystem> input_subsystem;
extern Shared<Core::Hardware::InterruptManager> interrupt_manager;
extern Shared<std::unordered_map<::pid_t, std::pair<size_t, Shared<Tegra::GPU>>>> gpus;
extern const Core::Reporter reporter;

inline void GrabGPU(::pid_t req_pid) {
    SharedWriter gpus_locked(gpus);
    auto it = gpus_locked->find(req_pid);
    if (it == gpus_locked->end()) {
        gpus_locked->emplace(req_pid, std::make_pair(1, VideoCore::CreateGPU()));
    } else {
        ++it->second.first; // increment ref count
    }
}

inline void PutGPU(::pid_t req_pid) {
    SharedWriter gpus_locked(gpus);
    auto it = gpus_locked->find(req_pid);
    if (it == gpus_locked->end()) {
        LOG_ERROR(Service_NVDRV, "PutGPU on non-existent or already-erased entry at {}", req_pid);
        return;
    }
    if (--it->second.first == 0) {
        gpus_locked->erase(it);
    }
}

inline Shared<Tegra::GPU>& GPU(::pid_t req_pid) {
    return const_cast<Service::Shared<Tegra::GPU>&>(SharedReader(gpus)->at(req_pid).second);
}

inline u64 GetProcessID()
{
    LOG_CRITICAL(Service, "mizu TODO GetProcessID");
    pid_t pid = mizu_servctl(MIZU_SCTL_GET_PROCESS_ID);
    if (pid == -1) {
        LOG_CRITICAL(Service, "MIZU_SCTL_GET_PROCESS_ID failed: {}", ResultCode(errno).description.Value());
    }
    return pid; // TODO TEMP
}

inline u64 GetTitleID()
{
    LOG_CRITICAL(Service, "mizu TODO GetTitleId");
    return 0; // TODO TEMP
}

using CurrentBuildProcessID = std::array<u8, 0x20>;
inline const CurrentBuildProcessID& GetCurrentProcessBuildID() {
    static CurrentBuildProcessID build_id = { 0 }; // TODO TEMP
    LOG_CRITICAL(Service, "mizu TODO GetCurrentProcessBuildID");
    return build_id;
}

inline std::chrono::nanoseconds GetGlobalTimeNs() {
    ::timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == -1) {
        LOG_CRITICAL(Service, "mizu TODO GetCurrentProcessBuildID");
	return {};
    }
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::seconds{ts.tv_sec} + std::chrono::nanoseconds{ts.tv_nsec});
}
inline std::chrono::microseconds GetGlobalTimeUs() {
    return std::chrono::duration_cast<std::chrono::microseconds>(GetGlobalTimeNs());
}

extern thread_local std::unordered_set<std::shared_ptr<Kernel::SessionRequestManager>> session_managers;

inline unsigned long AddSessionManager(std::shared_ptr<Kernel::SessionRequestManager> manager) {
    return (unsigned long)&**session_managers.insert(std::move(manager)).first;
}
inline unsigned long AddSessionManager(Kernel::SessionRequestHandlerPtr handler) {
    auto manager = std::make_shared<Kernel::SessionRequestManager>();
    manager->SetSessionHandler(std::move(handler));
    return AddSessionManager(std::move(manager));
}
inline decltype(session_managers)::iterator FindSessionManager(unsigned long session_id) {
	auto is_session = [=](const std::shared_ptr<Kernel::SessionRequestManager>& mgr){
		return &*mgr == (Kernel::SessionRequestManager *)session_id;
	};
	return std::find_if(session_managers.begin(), session_managers.end(),
			    is_session);
}

/// Default number of maximum connections to a server session.
static constexpr u32 ServerSessionCountMax = 0x40;
static_assert(ServerSessionCountMax == 0x40,
              "ServerSessionCountMax isn't 0x40 somehow, this assert is a reminder that this will "
              "break lots of things");

/**
 * This is an non-templated base of ServiceFramework to reduce code bloat and compilation times, it
 * is not meant to be used directly.
 *
 * @see ServiceFramework
 */
class ServiceFrameworkBase : public Kernel::SessionRequestHandler {
public:
    /// Returns the string identifier used to connect to the service.
    std::string GetServiceName() const {
        return service_name;
    }

    /**
     * Returns the maximum number of sessions that can be connected to this service at the same
     * time.
     */
    u32 GetMaxSessions() const {
        return max_sessions;
    }

    /// Invokes a service request routine using the HIPC protocol.
    void InvokeRequest(Kernel::HLERequestContext& ctx);

    /// Invokes a service request routine using the HIPC protocol.
    void InvokeRequestTipc(Kernel::HLERequestContext& ctx);

    /// Handles a synchronization request for the service.
    ResultCode HandleSyncRequest(Kernel::HLERequestContext& context);

protected:
    /// Member-function pointer type of SyncRequest handlers.
    template <typename Self>
    using HandlerFnP = void (Self::*)(Kernel::HLERequestContext&);

    /// Used to gain exclusive access to the service members, e.g. from CoreTiming thread.
    [[nodiscard]] std::scoped_lock<Common::SpinLock> LockService() {
        return std::scoped_lock{lock_service};
    }

    /// Identifier string used to connect to the service.
    std::string service_name;

private:
    template <typename T>
    friend class ServiceFramework;

    struct FunctionInfoBase {
        u32 expected_header;
        HandlerFnP<ServiceFrameworkBase> handler_callback;
        const char* name;
    };

    using InvokerFn = void(ServiceFrameworkBase* object, HandlerFnP<ServiceFrameworkBase> member,
                           Kernel::HLERequestContext& ctx);

    explicit ServiceFrameworkBase(const char* service_name_,
                                  u32 max_sessions_, InvokerFn* handler_invoker_);
    ~ServiceFrameworkBase();

    void RegisterHandlersBase(const FunctionInfoBase* functions, std::size_t n);
    void RegisterHandlersBaseTipc(const FunctionInfoBase* functions, std::size_t n);
    void ReportUnimplementedFunction(Kernel::HLERequestContext& ctx, const FunctionInfoBase* info);

    /// Maximum number of concurrent sessions that this service can handle.
    u32 max_sessions;

    /// Flag to store if a port was already create/installed to detect multiple install attempts,
    /// which is not supported.
    bool service_registered = false;

    /// Function used to safely up-cast pointers to the derived class before invoking a handler.
    InvokerFn* handler_invoker;
    boost::container::flat_map<u32, FunctionInfoBase> handlers;
    boost::container::flat_map<u32, FunctionInfoBase> handlers_tipc;

    /// Used to gain exclusive access to the service members, e.g. from CoreTiming thread.
    Common::SpinLock lock_service;
};

/**
 * Framework for implementing HLE services. Dispatches on the header id of incoming SyncRequests
 * based on a table mapping header ids to handler functions. Service implementations should inherit
 * from ServiceFramework using the CRTP (`class Foo : public ServiceFramework<Foo> { ... };`) and
 * populate it with handlers by calling #RegisterHandlers.
 *
 * In order to avoid duplicating code in the binary and exposing too many implementation details in
 * the header, this class is split into a non-templated base (ServiceFrameworkBase) and a template
 * deriving from it (ServiceFramework). The functions in this class will mostly only erase the type
 * of the passed in function pointers and then delegate the actual work to the implementation in the
 * base class.
 */
template <typename Self>
class ServiceFramework : public ServiceFrameworkBase {
protected:
    /// Contains information about a request type which is handled by the service.
    struct FunctionInfo : FunctionInfoBase {
        // TODO(yuriks): This function could be constexpr, but clang is the only compiler that
        // doesn't emit an ICE or a wrong diagnostic because of the static_cast.

        /**
         * Constructs a FunctionInfo for a function.
         *
         * @param expected_header_ request header in the command buffer which will trigger dispatch
         *     to this handler
         * @param handler_callback_ member function in this service which will be called to handle
         *     the request
         * @param name_ human-friendly name for the request. Used mostly for logging purposes.
         */
        FunctionInfo(u32 expected_header_, HandlerFnP<Self> handler_callback_, const char* name_)
            : FunctionInfoBase{
                  expected_header_,
                  // Type-erase member function pointer by casting it down to the base class.
                  static_cast<HandlerFnP<ServiceFrameworkBase>>(handler_callback_), name_} {}
    };

    /**
     * Initializes the handler with no functions installed.
     *
     * @param service_name_ Name of the service.
     * @param max_sessions_ Maximum number of sessions that can be
     *                      connected to this service at the same time.
     */
    explicit ServiceFramework(const char* service_name_,
                              u32 max_sessions_ = ServerSessionCountMax)
        : ServiceFrameworkBase(service_name_, max_sessions_, Invoker) {}

    /// Registers handlers in the service.
    template <std::size_t N>
    void RegisterHandlers(const FunctionInfo (&functions)[N]) {
        RegisterHandlers(functions, N);
    }

    /**
     * Registers handlers in the service. Usually prefer using the other RegisterHandlers
     * overload in order to avoid needing to specify the array size.
     */
    void RegisterHandlers(const FunctionInfo* functions, std::size_t n) {
        RegisterHandlersBase(functions, n);
    }

    /// Registers handlers in the service.
    template <std::size_t N>
    void RegisterHandlersTipc(const FunctionInfo (&functions)[N]) {
        RegisterHandlersTipc(functions, N);
    }

    /**
     * Registers handlers in the service. Usually prefer using the other RegisterHandlers
     * overload in order to avoid needing to specify the array size.
     */
    void RegisterHandlersTipc(const FunctionInfo* functions, std::size_t n) {
        RegisterHandlersBaseTipc(functions, n);
    }

private:
    /**
     * This function is used to allow invocation of pointers to handlers stored in the base class
     * without needing to expose the type of this derived class. Pointers-to-member may require a
     * fixup when being up or downcast, and thus code that does that needs to know the concrete type
     * of the derived class in order to invoke one of it's functions through a pointer.
     */
    static void Invoker(ServiceFrameworkBase* object, HandlerFnP<ServiceFrameworkBase> member,
                        Kernel::HLERequestContext& ctx) {
        // Cast back up to our original types and call the member function
        (static_cast<Self*>(object)->*static_cast<HandlerFnP<Self>>(member))(ctx);
    }
};

void StartServices();

/**
 * Service thread loop that runs forever.
 */
[[ noreturn ]] void RunForever(Kernel::SessionRequestHandlerPtr handler);

/// Creates service thread and regisers with the ServiceManager.
template <class T, typename... Args>
void MakeService(Args&&... args) {
    std::thread service([... args = std::forward<Args>(args)]() {
        pid_t tid = syscall(__NR_gettid);
        if (tid == -1) {
            LOG_CRITICAL(Service, "gettid failed: %s", ::strerror(errno));
            ::exit(1);
        }
        auto handler = std::static_pointer_cast<Service::ServiceFrameworkBase>(
                std::make_shared<T>(args...));
        if (SharedWriter(service_manager)->RegisterService(handler, tid) != ResultSuccess) {
            LOG_CRITICAL(Service, "RegisterService failed");
	    ::exit(1);
	}
	RunForever(static_cast<Kernel::SessionRequestHandlerPtr>(handler));
    });
    service.detach();
}

} // namespace Service
