// Copyright 2019 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
//
// Adapted by Kent Hall for mizu on Horizon Linux.

#include <ctime>
#include "common/logging/log.h"
#include "core/core.h"
#include "core/hardware_properties.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/service/sm/sm.h"
#include "core/hle/service/time/time.h"
#include "core/hle/service/time/time_interface.h"
#include "core/hle/service/time/time_manager.h"
#include "core/hle/service/time/time_sharedmemory.h"
#include "core/hle/service/time/time_zone_service.h"

namespace Service::Time {

class ISystemClock final : public ServiceFramework<ISystemClock> {
public:
    explicit ISystemClock(Clock::SystemClockCore& clock_core_)
        : ServiceFramework{"ISystemClock"}, clock_core{clock_core_} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, &ISystemClock::GetCurrentTime, "GetCurrentTime"},
            {1, nullptr, "SetCurrentTime"},
            {2,  &ISystemClock::GetSystemClockContext, "GetSystemClockContext"},
            {3, nullptr, "SetSystemClockContext"},
            {4, nullptr, "GetOperationEventReadableHandle"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }

private:
    void GetCurrentTime(Kernel::HLERequestContext& ctx) {
        LOG_DEBUG(Service_Time, "called");

        if (!clock_core.ReadLocked()->IsInitialized()) {
            IPC::ResponseBuilder rb{ctx, 2};
            rb.Push(ERROR_UNINITIALIZED_CLOCK);
            return;
        }

        s64 posix_time{};
        if (const ResultCode result{clock_core.ReadLocked()->GetCurrentTime(posix_time)};
            result.IsError()) {
            IPC::ResponseBuilder rb{ctx, 2};
            rb.Push(result);
            return;
        }

        IPC::ResponseBuilder rb{ctx, 4};
        rb.Push(ResultSuccess);
        rb.Push<s64>(posix_time);
    }

    void GetSystemClockContext(Kernel::HLERequestContext& ctx) {
        LOG_DEBUG(Service_Time, "called");

        if (!clock_core.ReadLocked()->IsInitialized()) {
            IPC::ResponseBuilder rb{ctx, 2};
            rb.Push(ERROR_UNINITIALIZED_CLOCK);
            return;
        }

        Clock::SystemClockContext system_clock_context{};
        if (const ResultCode result{clock_core.ReadLocked()->GetClockContext(system_clock_context)};
            result.IsError()) {
            IPC::ResponseBuilder rb{ctx, 2};
            rb.Push(result);
            return;
        }

        IPC::ResponseBuilder rb{ctx, sizeof(Clock::SystemClockContext) / 4 + 2};
        rb.Push(ResultSuccess);
        rb.PushRaw(system_clock_context);
    }

    Clock::SystemClockCore& clock_core;
};

class ISteadyClock final : public ServiceFramework<ISteadyClock> {
public:
    explicit ISteadyClock(Clock::SteadyClockCore& clock_core_)
        : ServiceFramework{"ISteadyClock"}, clock_core{clock_core_} {
        static const FunctionInfo functions[] = {
            {0, &ISteadyClock::GetCurrentTimePoint, "GetCurrentTimePoint"},
            {2, nullptr, "GetTestOffset"},
            {3, nullptr, "SetTestOffset"},
            {100, nullptr, "GetRtcValue"},
            {101, nullptr, "IsRtcResetDetected"},
            {102, nullptr, "GetSetupResultValue"},
            {200, nullptr, "GetInternalOffset"},
            {201, nullptr, "SetInternalOffset"},
        };
        RegisterHandlers(functions);
    }

private:
    void GetCurrentTimePoint(Kernel::HLERequestContext& ctx) {
        LOG_DEBUG(Service_Time, "called");

        if (!clock_core.IsInitialized()) {
            IPC::ResponseBuilder rb{ctx, 2};
            rb.Push(ERROR_UNINITIALIZED_CLOCK);
            return;
        }

        const Clock::SteadyClockTimePoint time_point{clock_core.GetCurrentTimePoint()};
        IPC::ResponseBuilder rb{ctx, (sizeof(Clock::SteadyClockTimePoint) / 4) + 2};
        rb.Push(ResultSuccess);
        rb.PushRaw(time_point);
    }

    Clock::SteadyClockCore& clock_core;
};

ResultCode Module::Interface::GetClockSnapshotFromSystemClockContextInternal(
    Clock::SystemClockContext user_context,
    Clock::SystemClockContext network_context, Clock::TimeType type,
    Clock::ClockSnapshot& clock_snapshot) {

    auto& time_manager{module->time_manager};
    clock_snapshot.steady_clock_time_point =
        time_manager.GetStandardSteadyClockCore().WriteLocked()->GetCurrentTimePoint();
    clock_snapshot.is_automatic_correction_enabled =
        time_manager.GetStandardUserSystemClockCore().ReadLocked()->IsAutomaticCorrectionEnabled();
    clock_snapshot.type = type;

    if (const ResultCode result{
            time_manager.GetTimeZoneContentManager().GetTimeZoneManager().GetDeviceLocationName(
                clock_snapshot.location_name)};
        result != ResultSuccess) {
        return result;
    }

    clock_snapshot.user_context = user_context;

    if (const ResultCode result{Clock::ClockSnapshot::GetCurrentTime(
            clock_snapshot.user_time, clock_snapshot.steady_clock_time_point,
            clock_snapshot.user_context)};
        result != ResultSuccess) {
        return result;
    }

    TimeZone::CalendarInfo userCalendarInfo{};
    if (const ResultCode result{
            time_manager.GetTimeZoneContentManager().GetTimeZoneManager().ToCalendarTimeWithMyRules(
                clock_snapshot.user_time, userCalendarInfo)};
        result != ResultSuccess) {
        return result;
    }

    clock_snapshot.user_calendar_time = userCalendarInfo.time;
    clock_snapshot.user_calendar_additional_time = userCalendarInfo.additional_info;

    clock_snapshot.network_context = network_context;

    if (Clock::ClockSnapshot::GetCurrentTime(clock_snapshot.network_time,
                                             clock_snapshot.steady_clock_time_point,
                                             clock_snapshot.network_context) != ResultSuccess) {
        clock_snapshot.network_time = 0;
    }

    TimeZone::CalendarInfo networkCalendarInfo{};
    if (const ResultCode result{
            time_manager.GetTimeZoneContentManager().GetTimeZoneManager().ToCalendarTimeWithMyRules(
                clock_snapshot.network_time, networkCalendarInfo)};
        result != ResultSuccess) {
        return result;
    }

    clock_snapshot.network_calendar_time = networkCalendarInfo.time;
    clock_snapshot.network_calendar_additional_time = networkCalendarInfo.additional_info;

    return ResultSuccess;
}

void Module::Interface::GetStandardUserSystemClock(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called");
    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<ISystemClock>(module->time_manager.GetStandardUserSystemClockCore());
}

void Module::Interface::GetStandardNetworkSystemClock(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called");
    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<ISystemClock>(module->time_manager.GetStandardNetworkSystemClockCore());
}

void Module::Interface::GetStandardSteadyClock(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called");
    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<ISteadyClock>(module->time_manager.GetStandardSteadyClockCore());
}

void Module::Interface::GetTimeZoneService(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called");
    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<ITimeZoneService>(module->time_manager.GetTimeZoneContentManager());
}

void Module::Interface::GetStandardLocalSystemClock(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called");
    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<ISystemClock>(module->time_manager.GetStandardLocalSystemClockCore());
}

void Module::Interface::IsStandardNetworkSystemClockAccuracySufficient(
    Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called");
    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push<u32>(module->time_manager.GetStandardNetworkSystemClockCore().ReadLocked()
            ->IsStandardNetworkSystemClockAccuracySufficient());
}

void Module::Interface::CalculateMonotonicSystemClockBaseTimePoint(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called");

    if (!module->time_manager.GetStandardSteadyClockCore().ReadLocked()->IsInitialized()) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ERROR_UNINITIALIZED_CLOCK);
        return;
    }

    IPC::RequestParser rp{ctx};
    const auto context{rp.PopRaw<Clock::SystemClockContext>()};
    const auto current_time_point{
        module->time_manager.GetStandardSteadyClockCore().WriteLocked()->GetCurrentTimePoint()};

    if (current_time_point.clock_source_id == context.steady_time_point.clock_source_id) {
        const auto ticks{Clock::TimeSpanType::FromTicks(::clock(),
                                                        Core::Hardware::CNTFREQ)};
        const s64 base_time_point{context.offset + current_time_point.time_point -
                                  ticks.ToSeconds()};
        IPC::ResponseBuilder rb{ctx, (sizeof(s64) / 4) + 2};
        rb.Push(ResultSuccess);
        rb.PushRaw(base_time_point);
        return;
    }

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ERROR_TIME_MISMATCH);
}

void Module::Interface::GetClockSnapshot(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto type{rp.PopEnum<Clock::TimeType>()};

    LOG_DEBUG(Service_Time, "called, type={}", type);

    Clock::SystemClockContext user_context{};
    if (const ResultCode result{
            module->time_manager.GetStandardUserSystemClockCore().WriteLocked()
                ->GetClockContext(user_context)};
        result.IsError()) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(result);
        return;
    }

    Clock::SystemClockContext network_context{};
    if (const ResultCode result{
            module->time_manager.GetStandardNetworkSystemClockCore().ReadLocked()
                ->GetClockContext(network_context)};
        result.IsError()) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(result);
        return;
    }

    Clock::ClockSnapshot clock_snapshot{};
    if (const ResultCode result{GetClockSnapshotFromSystemClockContextInternal(
            user_context, network_context, type, clock_snapshot)};
        result.IsError()) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(result);
        return;
    }

    ctx.WriteBuffer(clock_snapshot);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void Module::Interface::GetClockSnapshotFromSystemClockContext(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto type{rp.PopEnum<Clock::TimeType>()};

    rp.Skip(1, false);

    const Clock::SystemClockContext user_context{rp.PopRaw<Clock::SystemClockContext>()};
    const Clock::SystemClockContext network_context{rp.PopRaw<Clock::SystemClockContext>()};

    LOG_DEBUG(Service_Time, "called, type={}", type);

    Clock::ClockSnapshot clock_snapshot{};
    if (const ResultCode result{GetClockSnapshotFromSystemClockContextInternal(
            user_context, network_context, type, clock_snapshot)};
        result != ResultSuccess) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(result);
        return;
    }

    ctx.WriteBuffer(clock_snapshot);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void Module::Interface::CalculateStandardUserSystemClockDifferenceByUser(
    Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called");

    Clock::ClockSnapshot snapshot_a;
    Clock::ClockSnapshot snapshot_b;

    const auto snapshot_a_data = ctx.ReadBuffer(0);
    const auto snapshot_b_data = ctx.ReadBuffer(1);

    std::memcpy(&snapshot_a, snapshot_a_data.data(), sizeof(Clock::ClockSnapshot));
    std::memcpy(&snapshot_b, snapshot_b_data.data(), sizeof(Clock::ClockSnapshot));

    auto time_span_type{Clock::TimeSpanType::FromSeconds(snapshot_b.user_context.offset -
                                                         snapshot_a.user_context.offset)};

    if ((snapshot_b.user_context.steady_time_point.clock_source_id !=
         snapshot_a.user_context.steady_time_point.clock_source_id) ||
        (snapshot_b.is_automatic_correction_enabled &&
         snapshot_a.is_automatic_correction_enabled)) {
        time_span_type.nanoseconds = 0;
    }

    IPC::ResponseBuilder rb{ctx, (sizeof(s64) / 4) + 2};
    rb.Push(ResultSuccess);
    rb.PushRaw(time_span_type.nanoseconds);
}

void Module::Interface::CalculateSpanBetween(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called");

    Clock::ClockSnapshot snapshot_a;
    Clock::ClockSnapshot snapshot_b;

    const auto snapshot_a_data = ctx.ReadBuffer(0);
    const auto snapshot_b_data = ctx.ReadBuffer(1);

    std::memcpy(&snapshot_a, snapshot_a_data.data(), sizeof(Clock::ClockSnapshot));
    std::memcpy(&snapshot_b, snapshot_b_data.data(), sizeof(Clock::ClockSnapshot));

    Clock::TimeSpanType time_span_type{};
    s64 span{};

    if (const ResultCode result{snapshot_a.steady_clock_time_point.GetSpanBetween(
            snapshot_b.steady_clock_time_point, span)};
        result != ResultSuccess) {
        if (snapshot_a.network_time && snapshot_b.network_time) {
            time_span_type =
                Clock::TimeSpanType::FromSeconds(snapshot_b.network_time - snapshot_a.network_time);
        } else {
            IPC::ResponseBuilder rb{ctx, 2};
            rb.Push(ERROR_TIME_NOT_FOUND);
            return;
        }
    } else {
        time_span_type = Clock::TimeSpanType::FromSeconds(span);
    }

    IPC::ResponseBuilder rb{ctx, (sizeof(s64) / 4) + 2};
    rb.Push(ResultSuccess);
    rb.PushRaw(time_span_type.nanoseconds);
}

void Module::Interface::GetSharedMemoryNativeHandle(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called");
    IPC::ResponseBuilder rb{ctx, 2, 1};
    rb.Push(ResultSuccess);
    rb.PushCopyFds(module->time_manager.GetSharedMemory().GetFd());
}

Module::Interface::Interface(std::shared_ptr<Module> module_,
                             const char* name)
    : ServiceFramework{name}, module{std::move(module_)} {}

Module::Interface::~Interface() = default;

void InstallInterfaces() {
    // shared module state is independently synchronized
    auto module{std::make_shared<Module>()};
    MakeService<Time>(module, "time:a");
    MakeService<Time>(module, "time:s");
    MakeService<Time>(module, "time:u");
}

} // namespace Service::Time
