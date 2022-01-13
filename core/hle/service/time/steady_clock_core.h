// Copyright 2019 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <shared_mutex>
#include "common/uuid.h"
#include "core/hle/service/service.h"
#include "core/hle/service/time/clock_types.h"

namespace Core {
class System;
}

namespace Service::Time::Clock {

class SteadyClockCore {
public:
    SteadyClockCore() = default;
    virtual ~SteadyClockCore() = default;

    const Common::UUID& GetClockSourceId() const {
        return clock_source_id;
    }

    void SetClockSourceId(const Common::UUID& value) {
        clock_source_id = value;
    }

    virtual TimeSpanType GetInternalOffset() const = 0;

    virtual void SetInternalOffset(TimeSpanType internal_offset) = 0;

    virtual SteadyClockTimePoint GetTimePoint() = 0;

    virtual TimeSpanType GetCurrentRawTimePoint() = 0;

    SteadyClockTimePoint GetCurrentTimePoint() {
        SteadyClockTimePoint result{GetTimePoint()};
        result.time_point += GetInternalOffset().ToSeconds();
        return result;
    }

    bool IsInitialized() const {
        return is_initialized;
    }

    void MarkAsInitialized() {
        is_initialized = true;
    }

    auto ReadLocked() { return Service::SharedReader(mtx, *this); };
    auto WriteLocked() { return Service::SharedWriter(mtx, *this); };

protected:
    std::shared_mutex mtx;

private:
    Common::UUID clock_source_id{Common::UUID::Generate()};
    bool is_initialized{};
};

template <class Derived>
class SteadyClockCoreLocked : public SteadyClockCore {
public:
    auto ReadLocked() { return Service::SharedReader(mtx, *static_cast<const Derived *>(this)); };
    auto WriteLocked() { return Service::SharedWriter(mtx, *static_cast<Derived *>(this)); };
};

} // namespace Service::Time::Clock
