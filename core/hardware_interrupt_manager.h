// Copyright 2019 Yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
//
// Adapted by Kent Hall for mizu on Horizon Linux.

#pragma once

#include <memory>
#include <ctime>

#include "common/common_types.h"

namespace Core::Hardware {

class InterruptManager {
public:
    explicit InterruptManager();
    ~InterruptManager();

    void GPUInterruptSyncpt(u32 syncpoint_id, u32 value);

private:
    ::timer_t gpu_interrupt_event = nullptr;
};

} // namespace Core::Hardware
