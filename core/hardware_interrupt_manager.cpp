// Copyright 2019 Yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <csignal>
#include "core/core.h"
#include "core/core_timing.h"
#include "core/hardware_interrupt_manager.h"
#include "core/hle/service/kernel_helpers.h"
#include "core/hle/service/nvdrv/nvdrv_interface.h"
#include "core/hle/service/sm/sm.h"

namespace Core::Hardware {

InterruptManager::InterruptManager() {}

InterruptManager::~InterruptManager() {
    if (gpu_interrupt_event) {
        Service::KernelHelpers::CloseTimerEvent(gpu_interrupt_event);
    }
}

void InterruptManager::GPUInterruptSyncpt(const u32 syncpoint_id, const u32 value) {
    if (gpu_interrupt_event) {
        ::itimerspec curr_val;
        if (::timer_gettime(gpu_interrupt_event, &curr_val) != -1 &&
            (curr_val.it_value.tv_sec != 0 || curr_val.it_value.tv_nsec != 0)) {
            // timer is already armed, ignore this call
            return;
        } else {
            Service::KernelHelpers::CloseTimerEvent(gpu_interrupt_event);
        }
    }
    const u64 msg = (static_cast<u64>(syncpoint_id) << 32ULL) | value;
    gpu_interrupt_event = Service::KernelHelpers::CreateTimerEvent(
        "GPUInterrupt",
        reinterpret_cast<void *>(msg),
        [](::sigval sigev_value) {
            uintptr_t message = reinterpret_cast<uintptr_t>(sigev_value.sival_ptr);
            auto nvdrv = Service::SharedReader(Service::service_manager)->GetService<Service::Nvidia::NVDRV>("nvdrv");
            const u32 syncpt = static_cast<u32>(message >> 32);
            const u32 value = static_cast<u32>(message);
            nvdrv->SignalGPUInterruptSyncpt(syncpt, value);
        });
    Service::KernelHelpers::ScheduleTimerEvent(std::chrono::nanoseconds{10}, gpu_interrupt_event);
}

} // namespace Core::Hardware
