// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
//
// Adapted by Kent Hall for mizu on Horizon Linux.

#pragma once

#include "core/hle/service/service.h"
#include "core/hle/service/time/clock_types.h"
#include "core/hle/service/time/time_manager.h"

namespace Service::Time {

class Module final {
public:
    Module()
        : time_manager{} {}

    class Interface : public ServiceFramework<Interface> {
    public:
        explicit Interface(std::shared_ptr<Module> module_,
                           const char* name);
        ~Interface() override;

        void GetStandardUserSystemClock(Kernel::HLERequestContext& ctx);
        void GetStandardNetworkSystemClock(Kernel::HLERequestContext& ctx);
        void GetStandardSteadyClock(Kernel::HLERequestContext& ctx);
        void GetTimeZoneService(Kernel::HLERequestContext& ctx);
        void GetStandardLocalSystemClock(Kernel::HLERequestContext& ctx);
        void IsStandardNetworkSystemClockAccuracySufficient(Kernel::HLERequestContext& ctx);
        void CalculateMonotonicSystemClockBaseTimePoint(Kernel::HLERequestContext& ctx);
        void GetClockSnapshot(Kernel::HLERequestContext& ctx);
        void GetClockSnapshotFromSystemClockContext(Kernel::HLERequestContext& ctx);
        void CalculateStandardUserSystemClockDifferenceByUser(Kernel::HLERequestContext& ctx);
        void CalculateSpanBetween(Kernel::HLERequestContext& ctx);
        void GetSharedMemoryNativeHandle(Kernel::HLERequestContext& ctx);

    private:
        ResultCode GetClockSnapshotFromSystemClockContextInternal(
            Clock::SystemClockContext user_context,
            Clock::SystemClockContext network_context, Clock::TimeType type,
            Clock::ClockSnapshot& cloc_snapshot);

    protected:
        std::shared_ptr<Module> module;
    };

    TimeManager time_manager;
};

/// Registers all Time services with the specified service manager.
void InstallInterfaces();

} // namespace Service::Time
