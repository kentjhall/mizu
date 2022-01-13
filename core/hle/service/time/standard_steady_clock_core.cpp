// Copyright 2019 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <ctime>
#include "core/core.h"
#include "core/hardware_properties.h"
#include "core/hle/service/time/standard_steady_clock_core.h"

namespace Service::Time::Clock {

TimeSpanType StandardSteadyClockCore::GetCurrentRawTimePoint() {
    const TimeSpanType ticks_time_span{
        TimeSpanType::FromTicks(::clock(), Core::Hardware::CNTFREQ)};
    TimeSpanType raw_time_point{setup_value.nanoseconds + ticks_time_span.nanoseconds};

    if (raw_time_point.nanoseconds < cached_raw_time_point.nanoseconds) {
        raw_time_point.nanoseconds = cached_raw_time_point.nanoseconds;
    }

    cached_raw_time_point = raw_time_point;
    return raw_time_point;
}

} // namespace Service::Time::Clock
