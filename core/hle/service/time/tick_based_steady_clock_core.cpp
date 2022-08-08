// Copyright 2020 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
//
// Adapted by Kent Hall for mizu on Horizon Linux.

#include <ctime>
#include "core/core.h"
#include "core/hardware_properties.h"
#include "core/hle/service/time/tick_based_steady_clock_core.h"

namespace Service::Time::Clock {

SteadyClockTimePoint TickBasedSteadyClockCore::GetTimePoint() {
    const TimeSpanType ticks_time_span{
        TimeSpanType::FromTicks(::clock(), Core::Hardware::CNTFREQ)};

    return {ticks_time_span.ToSeconds(), GetClockSourceId()};
}

TimeSpanType TickBasedSteadyClockCore::GetCurrentRawTimePoint() {
    return TimeSpanType::FromSeconds(GetTimePoint().time_point);
}

} // namespace Service::Time::Clock
