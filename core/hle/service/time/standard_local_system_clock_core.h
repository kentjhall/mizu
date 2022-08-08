// Copyright 2019 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
//
// Adapted by Kent Hall for mizu on Horizon Linux.

#pragma once

#include "core/hle/service/time/system_clock_core.h"

namespace Service::Time::Clock {

class StandardLocalSystemClockCore final : public SystemClockCoreLocked<StandardLocalSystemClockCore> {
public:
    explicit StandardLocalSystemClockCore(SteadyClockCore& steady_clock_core_)
        : SystemClockCoreLocked<StandardLocalSystemClockCore>{steady_clock_core_} {}
};

} // namespace Service::Time::Clock
