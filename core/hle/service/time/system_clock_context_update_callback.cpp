// Copyright 2019 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/kernel_helpers.h"
#include "core/hle/service/time/errors.h"
#include "core/hle/service/time/system_clock_context_update_callback.h"

namespace Service::Time::Clock {

SystemClockContextUpdateCallback::SystemClockContextUpdateCallback() = default;
SystemClockContextUpdateCallback::~SystemClockContextUpdateCallback() = default;

bool SystemClockContextUpdateCallback::NeedUpdate(const SystemClockContext& value) const {
    if (has_context) {
        return context.offset != value.offset ||
               context.steady_time_point.clock_source_id != value.steady_time_point.clock_source_id;
    }

    return true;
}

void SystemClockContextUpdateCallback::RegisterOperationEvent(int event) {
    operation_event_list.push_back(event);
}

void SystemClockContextUpdateCallback::BroadcastOperationEvent() {
    for (auto event : operation_event_list) {
        KernelHelpers::SignalEvent(event);
    }
}

ResultCode SystemClockContextUpdateCallback::Update(const SystemClockContext& value) {
    ResultCode result{ResultSuccess};

    if (NeedUpdate(value)) {
        context = value;
        has_context = true;

        result = Update();

        if (result == ResultSuccess) {
            BroadcastOperationEvent();
        }
    }

    return result;
}

ResultCode SystemClockContextUpdateCallback::Update() {
    return ResultSuccess;
}

} // namespace Service::Time::Clock
