// Copyright 2019 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>

#include "common/common_types.h"
#include "core/hle/service/service.h"
#include "core/hle/service/time/clock_types.h"

namespace Service::Time::Clock {

class SteadyClockCore;
class SystemClockContextUpdateCallback;

// Parts of this implementation were based on Ryujinx (https://github.com/Ryujinx/Ryujinx/pull/783).
// This code was released under public domain.

class SystemClockCore {
public:
    explicit SystemClockCore(SteadyClockCore& steady_clock_core_);
    virtual ~SystemClockCore();

    SteadyClockCore& GetSteadyClockCore() const {
        return steady_clock_core;
    }

    ResultCode GetCurrentTime(s64& posix_time) const;

    ResultCode SetCurrentTime(s64 posix_time);

    virtual ResultCode GetClockContext(SystemClockContext& value) const {
        value = context;
        return ResultSuccess;
    }

    virtual ResultCode SetClockContext(const SystemClockContext& value) {
        context = value;
        return ResultSuccess;
    }

    virtual ResultCode Flush(const SystemClockContext& clock_context);

    void SetUpdateCallbackInstance(std::shared_ptr<SystemClockContextUpdateCallback> callback) {
        system_clock_context_update_callback = std::move(callback);
    }

    ResultCode SetSystemClockContext(const SystemClockContext& context);

    bool IsInitialized() const {
        return is_initialized;
    }

    void MarkAsInitialized() {
        is_initialized = true;
    }

    bool IsClockSetup() const;

    auto ReadLocked() { return Service::SharedReader(mtx, *this); };
    auto WriteLocked() { return Service::SharedWriter(mtx, *this); };

protected:
    std::shared_mutex mtx;

private:
    SteadyClockCore& steady_clock_core;
    SystemClockContext context{};
    bool is_initialized{};
    std::shared_ptr<SystemClockContextUpdateCallback> system_clock_context_update_callback;
};

template <class Derived>
class SystemClockCoreLocked : public SystemClockCore {
public:
    explicit SystemClockCoreLocked(SteadyClockCore& steady_clock_core_)
        : SystemClockCore(steady_clock_core_) {}

    auto ReadLocked() { return Service::SharedReader(mtx, *static_cast<const Derived *>(this)); };
    auto WriteLocked() { return Service::SharedWriter(mtx, *static_cast<Derived *>(this)); };
};

} // namespace Service::Time::Clock
