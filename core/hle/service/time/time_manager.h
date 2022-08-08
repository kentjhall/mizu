// Copyright 2019 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
//
// Adapted by Kent Hall for mizu on Horizon Linux.

#pragma once

#include "common/common_types.h"
#include "common/time_zone.h"
#include "core/file_sys/vfs_types.h"
#include "core/hle/service/time/clock_types.h"
#include "core/hle/service/time/ephemeral_network_system_clock_core.h"
#include "core/hle/service/time/standard_local_system_clock_core.h"
#include "core/hle/service/time/standard_network_system_clock_core.h"
#include "core/hle/service/time/standard_steady_clock_core.h"
#include "core/hle/service/time/standard_user_system_clock_core.h"
#include "core/hle/service/time/tick_based_steady_clock_core.h"
#include "core/hle/service/time/time_sharedmemory.h"
#include "core/hle/service/time/time_zone_content_manager.h"

namespace Service::Time {

namespace Clock {
class EphemeralNetworkSystemClockContextWriter;
class LocalSystemClockContextWriter;
class NetworkSystemClockContextWriter;
} // namespace Clock

// Parts of this implementation were based on Ryujinx (https://github.com/Ryujinx/Ryujinx/pull/783).
// This code was released under public domain.

class TimeManager final {
public:
    explicit TimeManager();
    ~TimeManager();

    Clock::StandardSteadyClockCore& GetStandardSteadyClockCore();

    const Clock::StandardSteadyClockCore& GetStandardSteadyClockCore() const;

    Clock::StandardLocalSystemClockCore& GetStandardLocalSystemClockCore();

    const Clock::StandardLocalSystemClockCore& GetStandardLocalSystemClockCore() const;

    Clock::StandardNetworkSystemClockCore& GetStandardNetworkSystemClockCore();

    const Clock::StandardNetworkSystemClockCore& GetStandardNetworkSystemClockCore() const;

    Clock::StandardUserSystemClockCore& GetStandardUserSystemClockCore();

    const Clock::StandardUserSystemClockCore& GetStandardUserSystemClockCore() const;

    const TimeZone::TimeZoneContentManager& GetTimeZoneContentManager() const;

    void UpdateLocalSystemClockTime(s64 posix_time);

    SharedMemory& GetSharedMemory();

    const SharedMemory& GetSharedMemory() const;

    void Shutdown();

    static s64 GetExternalTimeZoneOffset();

private:
    ;

    struct Impl;
    std::unique_ptr<Impl> impl;
    // time zones can only be initialized after impl is valid
    const TimeZone::TimeZoneContentManager time_zone_content_manager;
};

} // namespace Service::Time
