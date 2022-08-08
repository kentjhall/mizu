// Copyright 2019 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
//
// Adapted by Kent Hall for mizu on Horizon Linux.

#pragma once

#include "core/hle/service/time/clock_types.h"
#include "core/hle/service/time/steady_clock_core.h"
#include "core/hle/service/time/system_clock_core.h"

namespace Core {
class System;
}

namespace Service::Time::Clock {

class StandardNetworkSystemClockCore final : public SystemClockCoreLocked<StandardNetworkSystemClockCore> {
public:
    explicit StandardNetworkSystemClockCore(SteadyClockCore& steady_clock_core_)
        : SystemClockCoreLocked<StandardNetworkSystemClockCore>{steady_clock_core_} {}

    void SetStandardNetworkClockSufficientAccuracy(TimeSpanType value) {
        standard_network_clock_sufficient_accuracy = value;
    }

    bool IsStandardNetworkSystemClockAccuracySufficient() const {
        SystemClockContext clock_ctx{};
        if (GetClockContext(clock_ctx) != ResultSuccess) {
            return {};
        }

        s64 span{};
        if (clock_ctx.steady_time_point.GetSpanBetween(
                GetSteadyClockCore().WriteLocked()->GetCurrentTimePoint(), span) != ResultSuccess) {
            return {};
        }

        return TimeSpanType{span}.nanoseconds <
               standard_network_clock_sufficient_accuracy.nanoseconds;
    }

private:
    TimeSpanType standard_network_clock_sufficient_accuracy{};
};

} // namespace Service::Time::Clock
