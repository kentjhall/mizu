// Copyright 2019 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
//
// Adapted by Kent Hall for mizu on Horizon Linux.

#pragma once

#include <memory>
#include <vector>

#include "core/hle/service/time/clock_types.h"

namespace Service::Time::Clock {

// Parts of this implementation were based on Ryujinx (https://github.com/Ryujinx/Ryujinx/pull/783).
// This code was released under public domain.

class SystemClockContextUpdateCallback {
public:
    SystemClockContextUpdateCallback();
    virtual ~SystemClockContextUpdateCallback();

    bool NeedUpdate(const SystemClockContext& value) const;

    void RegisterOperationEvent(int writable_event);

    void BroadcastOperationEvent();

    ResultCode Update(const SystemClockContext& value);

protected:
    virtual ResultCode Update();

    SystemClockContext context{};

private:
    bool has_context{};
    std::vector<int> operation_event_list;
};

} // namespace Service::Time::Clock
