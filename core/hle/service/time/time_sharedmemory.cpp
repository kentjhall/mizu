// Copyright 2019 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <mutex>
#include <ctime>
#include <fcntl.h>
#include "core/core.h"
#include "core/hardware_properties.h"
#include "core/hle/service/time/clock_types.h"
#include "core/hle/service/time/steady_clock_core.h"
#include "core/hle/service/time/time_sharedmemory.h"

namespace Service::Time {

SharedMemory::SharedMemory()
    : shared_mem_fd{::memfd_create("mizu_time", MFD_ALLOW_SEALING)},
      shared_mem{nullptr, shared_mem_deleter} {
    if (shared_mem_fd == -1) {
        LOG_CRITICAL(Service_HID, "memfd_create failed: {}", ::strerror(errno));
    } else {
        if (::ftruncate(shared_mem_fd, SHARED_MEMORY_SIZE) == -1 ||
            ::fcntl(shared_mem_fd, F_ADD_SEALS, F_SEAL_SHRINK) == -1) {
            LOG_CRITICAL(Service_HID, "memfd setup failed: {}", ::strerror(errno));
        } else {
            u8 *shared_mapping = static_cast<u8 *>(::mmap(NULL, SHARED_MEMORY_SIZE, PROT_READ | PROT_WRITE,
                                                          MAP_SHARED, shared_mem_fd, 0));
            if (shared_mapping == MAP_FAILED) {
                LOG_CRITICAL(Service_HID, "mmap failed: {}", ::strerror(errno));
            } else {
                std::memset(shared_mapping, 0, SHARED_MEMORY_SIZE);
                shared_mem.reset(shared_mapping);
            }
        }
    }
}

SharedMemory::~SharedMemory() {
    ::close(shared_mem_fd);
}

void SharedMemory::SetupStandardSteadyClock(const Common::UUID& clock_source_id,
                                            Clock::TimeSpanType current_time_point) {
    const Clock::TimeSpanType ticks_time_span{Clock::TimeSpanType::FromTicks(
        ::clock(), Core::Hardware::CNTFREQ)};
    const Clock::SteadyClockContext context{
        static_cast<u64>(current_time_point.nanoseconds - ticks_time_span.nanoseconds),
        clock_source_id};
    std::unique_lock guard(shared_mem_mtx);
    shared_memory_format.standard_steady_clock_timepoint.StoreData(shared_mem.get(), context);
}

void SharedMemory::UpdateLocalSystemClockContext(const Clock::SystemClockContext& context) {
    std::unique_lock guard(shared_mem_mtx);
    shared_memory_format.standard_local_system_clock_context.StoreData(shared_mem.get(), context);
}

void SharedMemory::UpdateNetworkSystemClockContext(const Clock::SystemClockContext& context) {
    std::unique_lock guard(shared_mem_mtx);
    shared_memory_format.standard_network_system_clock_context.StoreData(shared_mem.get(), context);
}

void SharedMemory::SetAutomaticCorrectionEnabled(bool is_enabled) {
    std::unique_lock guard(shared_mem_mtx);
    shared_memory_format.standard_user_system_clock_automatic_correction.StoreData(shared_mem.get(), is_enabled);
}

} // namespace Service::Time
