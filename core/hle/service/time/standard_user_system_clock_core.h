// Copyright 2019 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
//
// Adapted by Kent Hall for mizu on Horizon Linux.

#pragma once

#include "core/hle/service/kernel_helpers.h"
#include "core/hle/service/time/clock_types.h"
#include "core/hle/service/time/system_clock_core.h"

namespace Service::Time::Clock {

class StandardLocalSystemClockCore;
class StandardNetworkSystemClockCore;

class StandardUserSystemClockCore final : public SystemClockCoreLocked<StandardUserSystemClockCore> {
public:
    StandardUserSystemClockCore(StandardLocalSystemClockCore& local_system_clock_core_,
                                StandardNetworkSystemClockCore& network_system_clock_core_);

    ~StandardUserSystemClockCore() override;

    ResultCode SetAutomaticCorrectionEnabled(bool value);

    ResultCode GetClockContext(SystemClockContext& ctx) const override;

    bool IsAutomaticCorrectionEnabled() const {
        return auto_correction_enabled;
    }

    void SetAutomaticCorrectionUpdatedTime(SteadyClockTimePoint steady_clock_time_point) {
        auto_correction_time = steady_clock_time_point;
    }

protected:
    ResultCode Flush(const SystemClockContext&) override;

    ResultCode SetClockContext(const SystemClockContext&) override;

    ResultCode ApplyAutomaticCorrection(bool value) const;

    const SteadyClockTimePoint& GetAutomaticCorrectionUpdatedTime() const {
        return auto_correction_time;
    }

private:
    StandardLocalSystemClockCore& local_system_clock_core;
    StandardNetworkSystemClockCore& network_system_clock_core;
    bool auto_correction_enabled{};
    SteadyClockTimePoint auto_correction_time;
    int auto_correction_event;
};

} // namespace Service::Time::Clock
