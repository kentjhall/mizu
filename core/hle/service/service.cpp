// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <fmt/format.h>
#include "common/assert.h"
#include "common/logging/log.h"
#include "common/settings.h"
#include "common/string_util.h"
#include "video_core/gpu.h"
#include "core/core.h"
#include "core/hardware_interrupt_manager.h"
#include "core/loader/loader.h"
#include "core/hle/ipc.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/svc_results.h"
#include "core/hle/service/acc/acc.h"
#include "core/hle/service/am/am.h"
#include "core/hle/service/am/applets/applets.h"
#include "core/hle/service/aoc/aoc_u.h"
#include "core/hle/service/apm/apm.h"
#include "core/hle/service/apm/apm_controller.h"
#include "core/hle/service/audio/audio.h"
#include "core/hle/service/bcat/bcat_module.h"
/* #include "core/hle/service/bpc/bpc.h" */
/* #include "core/hle/service/btdrv/btdrv.h" */
/* #include "core/hle/service/btm/btm.h" */
/* #include "core/hle/service/caps/caps.h" */
/* #include "core/hle/service/erpt/erpt.h" */
/* #include "core/hle/service/es/es.h" */
/* #include "core/hle/service/eupld/eupld.h" */
/* #include "core/hle/service/fatal/fatal.h" */
/* #include "core/hle/service/fgm/fgm.h" */
#include "core/hle/service/filesystem/filesystem.h"
#include "core/hle/service/friend/friend.h"
#include "core/hle/service/glue/glue.h"
#include "core/hle/service/glue/glue_manager.h"
/* #include "core/hle/service/grc/grc.h" */
#include "core/hle/service/hid/hid.h"
/* #include "core/hle/service/lbl/lbl.h" */
/* #include "core/hle/service/ldn/ldn.h" */
/* #include "core/hle/service/ldr/ldr.h" */
#include "core/hle/service/lm/lm.h"
/* #include "core/hle/service/mig/mig.h" */
/* #include "core/hle/service/mii/mii.h" */
/* #include "core/hle/service/mm/mm_u.h" */
/* #include "core/hle/service/ncm/ncm.h" */
/* #include "core/hle/service/nfc/nfc.h" */
/* #include "core/hle/service/nfp/nfp.h" */
/* #include "core/hle/service/ngct/ngct.h" */
/* #include "core/hle/service/nifm/nifm.h" */
/* #include "core/hle/service/nim/nim.h" */
/* #include "core/hle/service/npns/npns.h" */
#include "core/hle/service/ns/ns.h"
#include "core/hle/service/nvdrv/nvdrv.h"
#include "core/hle/service/nvflinger/nvflinger.h"
/* #include "core/hle/service/olsc/olsc.h" */
/* #include "core/hle/service/pcie/pcie.h" */
#include "core/hle/service/pctl/pctl_module.h"
/* #include "core/hle/service/pcv/pcv.h" */
/* #include "core/hle/service/pm/pm.h" */
/* #include "core/hle/service/prepo/prepo.h" */
/* #include "core/hle/service/psc/psc.h" */
#include "core/hle/service/ptm/psm.h"
#include "core/hle/service/service.h"
#include "core/hle/service/set/settings.h"
#include "core/hle/service/sm/sm.h"
/* #include "core/hle/service/sockets/sockets.h" */
/* #include "core/hle/service/spl/spl_module.h" */
/* #include "core/hle/service/ssl/ssl.h" */
#include "core/hle/service/time/time.h"
/* #include "core/hle/service/usb/usb.h" */
#include "core/hle/service/vi/vi.h"
/* #include "core/hle/service/wlan/wlan.h" */
#include "core/reporter.h"
#include "mizu_servctl.h"

namespace Service {

Shared<SM::ServiceManager> service_manager;
Shared<FileSystem::FileSystemController> filesystem_controller;
Shared<FileSys::ContentProviderUnion> content_provider;
Shared<FileSys::RealVfsFilesystem> filesystem;
Shared<APM::Controller> apm_controller;
Shared<AM::Applets::AppletManager> applet_manager;
Shared<NVFlinger::NVFlinger> nv_flinger;
Shared<Glue::ARPManager> arp_manager;
Shared<Core::Hardware::InterruptManager> interrupt_manager;
Shared<std::unordered_map<::pid_t, SharedGPU>> gpus;
const Core::Reporter reporter;

thread_local std::unordered_set<std::shared_ptr<Kernel::SessionRequestManager>> session_managers;

/**
 * Creates a function string for logging, complete with the name (or header code, depending
 * on what's passed in) the port name, and all the cmd_buff arguments.
 */
[[maybe_unused]] static std::string MakeFunctionString(std::string_view name,
                                                       std::string_view port_name,
                                                       const u32* cmd_buff) {
    // Number of params == bits 0-5 + bits 6-11
    int num_params = (cmd_buff[0] & 0x3F) + ((cmd_buff[0] >> 6) & 0x3F);

    std::string function_string = fmt::format("function '{}': port={}", name, port_name);
    for (int i = 1; i <= num_params; ++i) {
        function_string += fmt::format(", cmd_buff[{}]=0x{:X}", i, cmd_buff[i]);
    }
    return function_string;
}

ServiceFrameworkBase::ServiceFrameworkBase(const char* service_name_,
                                           u32 max_sessions_, InvokerFn* handler_invoker_)
    : SessionRequestHandler(service_name_),
      service_name{service_name_}, max_sessions{max_sessions_}, handler_invoker{handler_invoker_} {}

ServiceFrameworkBase::~ServiceFrameworkBase() {
    // Wait for other threads to release access before destroying
    const auto guard = LockService();
}

void ServiceFrameworkBase::RegisterHandlersBase(const FunctionInfoBase* functions, std::size_t n) {
    handlers.reserve(handlers.size() + n);
    for (std::size_t i = 0; i < n; ++i) {
        // Usually this array is sorted by id already, so hint to insert at the end
        handlers.emplace_hint(handlers.cend(), functions[i].expected_header, functions[i]);
    }
}

void ServiceFrameworkBase::RegisterHandlersBaseTipc(const FunctionInfoBase* functions,
                                                    std::size_t n) {
    handlers_tipc.reserve(handlers_tipc.size() + n);
    for (std::size_t i = 0; i < n; ++i) {
        // Usually this array is sorted by id already, so hint to insert at the end
        handlers_tipc.emplace_hint(handlers_tipc.cend(), functions[i].expected_header,
                                   functions[i]);
    }
}

void ServiceFrameworkBase::ReportUnimplementedFunction(Kernel::HLERequestContext& ctx,
                                                       const FunctionInfoBase* info) {
    auto cmd_buf = ctx.CommandBuffer();
    std::string function_name = info == nullptr ? fmt::format("{}", ctx.GetCommand()) : info->name;

    fmt::memory_buffer buf;
    fmt::format_to(std::back_inserter(buf), "function '{}': port='{}' cmd_buf={{[0]=0x{:X}",
                   function_name, service_name, cmd_buf[0]);
    for (int i = 1; i <= 8; ++i) {
        fmt::format_to(std::back_inserter(buf), ", [{}]=0x{:X}", i, cmd_buf[i]);
    }
    buf.push_back('}');

    UNIMPLEMENTED_MSG("Unknown / unimplemented {}", fmt::to_string(buf));
    if (Settings::values.use_auto_stub) {
        LOG_WARNING(Service, "Using auto stub fallback!");
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultSuccess);
    }
}

void ServiceFrameworkBase::InvokeRequest(Kernel::HLERequestContext& ctx) {
    auto itr = handlers.find(ctx.GetCommand());
    const FunctionInfoBase* info = itr == handlers.end() ? nullptr : &itr->second;
    if (info == nullptr || info->handler_callback == nullptr) {
        return ReportUnimplementedFunction(ctx, info);
    }

    LOG_TRACE(Service, "{}", MakeFunctionString(info->name, GetServiceName(), ctx.CommandBuffer()));
    handler_invoker(this, info->handler_callback, ctx);
}

void ServiceFrameworkBase::InvokeRequestTipc(Kernel::HLERequestContext& ctx) {
    boost::container::flat_map<u32, FunctionInfoBase>::iterator itr;

    itr = handlers_tipc.find(ctx.GetCommand());

    const FunctionInfoBase* info = itr == handlers_tipc.end() ? nullptr : &itr->second;
    if (info == nullptr || info->handler_callback == nullptr) {
        return ReportUnimplementedFunction(ctx, info);
    }

    LOG_TRACE(Service, "{}", MakeFunctionString(info->name, GetServiceName(), ctx.CommandBuffer()));
    handler_invoker(this, info->handler_callback, ctx);
}

ResultCode ServiceFrameworkBase::HandleSyncRequest(Kernel::HLERequestContext& ctx) {
    const auto guard = LockService();

    switch (ctx.GetCommandType()) {
    /*
     * Close/Control commands should be handled in the kernel.
     */
    case IPC::CommandType::ControlWithContext:
    case IPC::CommandType::Control: {
        SharedWriter(service_manager)->InvokeControlRequest(ctx);
        break;
    }
    case IPC::CommandType::RequestWithContext:
    case IPC::CommandType::Request: {
        InvokeRequest(ctx);
        break;
    }
    default:
        if (ctx.IsTipc()) {
            InvokeRequestTipc(ctx);
            break;
        }

        UNIMPLEMENTED_MSG("command_type={}", ctx.GetCommandType());
    }

    ctx.WriteToOutgoingCommandBuffer();

    return ResultSuccess;
}

/// Initialize Services
void StartServices() {
    // NVFlinger needs to be accessed by several services like Vi and AppletOE so we instantiate it
    // here and pass it into the respective InstallInterfaces functions.

    SharedWriter(filesystem_controller)->CreateFactories(false);

    /// Create default implementations of applets if one is not provided.
    SharedWriter(applet_manager)->SetDefaultAppletsIfMissing();

    /// Reset all glue registrations
    SharedWriter(arp_manager)->ResetAll();

    if (mizu_servctl(HZN_SCTL_REGISTER_NAMED_SERVICE, (unsigned long)"sm:") == -1) {
        LOG_CRITICAL(Service, "HZN_SCTL_REGISTER_NAMED_SERVICE failed");
        ::exit(1);
    }

    Account::InstallInterfaces();
    AM::InstallInterfaces();
    AOC::InstallInterfaces();
    APM::InstallInterfaces();
    Audio::InstallInterfaces();
    BCAT::InstallInterfaces();
    /* BPC::InstallInterfaces(*sm, system); */
    /* BtDrv::InstallInterfaces(*sm, system); */
    /* BTM::InstallInterfaces(*sm, system); */
    /* Capture::InstallInterfaces(*sm, system); */
    /* ERPT::InstallInterfaces(*sm, system); */
    /* ES::InstallInterfaces(*sm, system); */
    /* EUPLD::InstallInterfaces(*sm, system); */
    /* Fatal::InstallInterfaces(*sm, system); */
    /* FGM::InstallInterfaces(*sm, system); */
    FileSystem::InstallInterfaces();
    Friend::InstallInterfaces();
    Glue::InstallInterfaces();
    /* GRC::InstallInterfaces(*sm, system); */
    HID::InstallInterfaces();
    /* LBL::InstallInterfaces(*sm, system); */
    /* LDN::InstallInterfaces(*sm, system); */
    /* LDR::InstallInterfaces(*sm, system); */
    LM::InstallInterfaces();
    /* Migration::InstallInterfaces(*sm, system); */
    /* Mii::InstallInterfaces(*sm, system); */
    /* MM::InstallInterfaces(*sm, system); */
    /* NCM::InstallInterfaces(*sm, system); */
    /* NFC::InstallInterfaces(*sm, system); */
    /* NFP::InstallInterfaces(*sm, system); */
    /* NGCT::InstallInterfaces(*sm, system); */
    /* NIFM::InstallInterfaces(*sm, system); */
    /* NIM::InstallInterfaces(*sm, system); */
    /* NPNS::InstallInterfaces(*sm, system); */
    NS::InstallInterfaces();
    Nvidia::InstallInterfaces();
    /* OLSC::InstallInterfaces(*sm, system); */
    /* PCIe::InstallInterfaces(*sm, system); */
    PCTL::InstallInterfaces();
    /* PCV::InstallInterfaces(*sm, system); */
    /* PlayReport::InstallInterfaces(*sm, system); */
    /* PM::InstallInterfaces(system); */
    /* PSC::InstallInterfaces(*sm, system); */
    PSM::InstallInterfaces();
    Set::InstallInterfaces();
    /* Sockets::InstallInterfaces(*sm, system); */
    /* SPL::InstallInterfaces(*sm, system); */
    /* SSL::InstallInterfaces(*sm, system); */
    Time::InstallInterfaces();
    /* USB::InstallInterfaces(*sm, system); */
    VI::InstallInterfaces();
    /* WLAN::InstallInterfaces(*sm, system); */
}

[[ noreturn ]] void RunForever(Kernel::SessionRequestHandlerPtr handler)
{
    for (;;) {
        unsigned long session_id;

        long cmdptr = mizu_servctl(HZN_SCTL_GET_CMD, &session_id);
        if (cmdptr == -1) {
            ResultCode rc(errno);
            if (rc == Kernel::ResultCancelled) // this means EINTR
                continue;
            LOG_CRITICAL(Service, "Unexpected error on HZN_SCTL_GET_CMD: {}", rc.description.Value());
	    ::exit(1);
        }
	if (cmdptr == 0) {
                auto it = FindSessionManager(session_id);
                char name[20];
                pthread_getname_np(pthread_self(), name, sizeof(name));
                if (it == session_managers.end()) {
                    LOG_WARNING(Service,
                                "Unexpected session ID from HZN_SCTL_GET_CMD close request: {}",
                                session_id);
		} else {
                    session_managers.erase(it);
		}
		continue;
	}

        bool new_session = session_id == 0; // 0 means create a new session
        if (new_session) { 
            session_id = AddSessionManager(handler);
        }

        Kernel::SessionRequestManager *manager = (Kernel::SessionRequestManager *)session_id;

        u32* cmd_buf{reinterpret_cast<u32*>(cmdptr)};
        Kernel::HLERequestContext context(manager, cmd_buf);

        // If the session has been converted to a domain, handle the domain request
        if (manager->HasSessionRequestHandler(context)) {
            if (context.IsDomain() && context.HasDomainMessageHeader()) {
                if (!context.HasDomainMessageHeader()) {
                    goto out;
                }

                // If there is a DomainMessageHeader, then this is CommandType "Request"
                const auto& domain_message_header = context.GetDomainMessageHeader();
                const u32 object_id{domain_message_header.object_id};
                switch (domain_message_header.command) {
                case IPC::DomainMessageHeader::CommandType::SendMessage:
                    if (object_id > manager->DomainHandlerCount()) {
                        LOG_CRITICAL(IPC,
                                     "object_id {} is too big! This probably means a recent service call "
                                     "to (session={}) needed to return a new interface!",
                                     object_id, context.GetSessionId());
                        UNREACHABLE();
                        goto out; // Ignore error if asserts are off
                    }
                    manager->DomainHandler(object_id - 1)->HandleSyncRequest(context);
                    goto out;

                case IPC::DomainMessageHeader::CommandType::CloseVirtualHandle: {
                    LOG_DEBUG(IPC, "CloseVirtualHandle, object_id=0x{:08X}", object_id);

                    manager->CloseDomainHandler(object_id - 1);

                    IPC::ResponseBuilder rb{context, 2};
                    rb.Push(ResultSuccess);
                    goto out;
                }
                }

                LOG_CRITICAL(IPC, "Unknown domain command={}", domain_message_header.command.Value());
                ASSERT(false);
                goto out;
                // If there is no domain header, the regular session handler is used
            } else if (manager->HasSessionHandler()) {
                // If this ServerSession has an associated HLE handler, forward the request to it.
                manager->SessionHandler().HandleSyncRequest(context);
            }
        } else {
            ASSERT_MSG(false, "Session handler is invalid, stubbing response!");
            IPC::ResponseBuilder rb(context, 2);
            rb.Push(ResultSuccess);
        }
        
out:
        if (context.convert_to_domain) {
            ASSERT_MSG(!context.IsDomain(), "ServerSession is already a domain instance.");
            manager->ConvertToDomain();
            context.convert_to_domain = false;
        }

        if (mizu_servctl(HZN_SCTL_PUT_CMD, session_id, (long)context.IsDomain()) == -1) {
            // Just give up on the new session if this fails
            if (new_session) {
                session_managers.erase(FindSessionManager(session_id));
            }
            LOG_ERROR(Service, "HZN_SCTL_PUT_CMD failed: {}", ResultCode(errno).description.Value());
        }
    }
}

} // namespace Service
