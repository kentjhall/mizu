// Copyright 2019 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
//
// Adapted by Kent Hall for mizu on Horizon Linux.

#pragma once

#include "core/hle/service/time/system_clock_context_update_callback.h"

namespace Service::Time::Clock {

class EphemeralNetworkSystemClockContextWriter final : public SystemClockContextUpdateCallback {
public:
    EphemeralNetworkSystemClockContextWriter() : SystemClockContextUpdateCallback{} {}
};

} // namespace Service::Time::Clock
