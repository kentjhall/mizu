// Copyright 2019 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
//
// Adapted by Kent Hall for mizu on Horizon Linux.

#pragma once

#include "core/hle/service/time/errors.h"
#include "core/hle/service/time/system_clock_context_update_callback.h"
#include "core/hle/service/time/time_sharedmemory.h"

namespace Service::Time::Clock {

class LocalSystemClockContextWriter final : public SystemClockContextUpdateCallback {
public:
    explicit LocalSystemClockContextWriter(SharedMemory& shared_memory_)
        : SystemClockContextUpdateCallback{}, shared_memory{shared_memory_} {}

protected:
    ResultCode Update() override {
        shared_memory.UpdateLocalSystemClockContext(context);
        return ResultSuccess;
    }

private:
    SharedMemory& shared_memory;
};

} // namespace Service::Time::Clock
