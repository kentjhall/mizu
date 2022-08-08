// Copyright 2021 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
//
// Adapted by Kent Hall for mizu on Horizon Linux.

#include <chrono>
#include <ctime>
#include <cstring>
#include <unistd.h>
#include <pthread.h>
#include <sys/eventfd.h>
#include "common/thread.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/hle/service/kernel_helpers.h"

namespace Service::KernelHelpers {

void SetupServiceContext(std::string name) {
    Common::SetCurrentThreadName(name.c_str());
}

int CreateEvent(std::string&& name) {
    int fd = ::eventfd(0, EFD_NONBLOCK);
    if (fd == -1) {
        LOG_CRITICAL(Service, "eventfd failed: {}", ::strerror(errno));
    }
    return fd;
}

void CloseEvent(int efd) {
    ::close(efd);
}

void SignalEvent(int efd) {
    if (::eventfd_write(efd, 1) == -1) {
        LOG_CRITICAL(Service, "eventfd_write failed: {}", ::strerror(errno));
    }
}

void ClearEvent(int efd) {
    static eventfd_t whocares;
    if (::eventfd_read(efd, &whocares) == -1 && errno != EAGAIN) {
        LOG_CRITICAL(Service, "eventfd_read failed: {}", ::strerror(errno));
    }
}

::timer_t CreateTimerEvent(std::string name, void *val, void (*cb) (::sigval)) {
    ::sigevent sev;
    sev.sigev_notify = SIGEV_THREAD;
    sev.sigev_value = { .sival_ptr = val };
    sev.sigev_notify_function = cb;
    sev.sigev_notify_attributes = NULL;
    ::timer_t timerid;
    if (::timer_create(CLOCK_MONOTONIC, &sev, &timerid) == -1) {
        LOG_CRITICAL(Service, "timer_create failed: {}", ::strerror(errno));
        return nullptr;
    }
    return timerid;
}

void CloseTimerEvent(::timer_t event) {
    ::timer_delete(event);
}

void ScheduleRepeatTimerEvent(std::chrono::nanoseconds interval, ::timer_t event) {
    auto secs = std::chrono::duration_cast<std::chrono::seconds>(interval);
    interval -= secs;

    const ::itimerspec its = {
        .it_interval = {
            .tv_sec = secs.count(),
            .tv_nsec = interval.count(),
        },
        .it_value = {
            .tv_sec = secs.count(),
            .tv_nsec = interval.count(),
        },
    };

    if (timer_settime(event, 0, &its, NULL) == -1) {
        LOG_CRITICAL(Service, "timer_settime failed: {}", ::strerror(errno));
    }
}

void ScheduleTimerEvent(std::chrono::nanoseconds delay, ::timer_t event) {
    auto secs = std::chrono::duration_cast<std::chrono::seconds>(delay);
    delay -= secs;

    const ::itimerspec its = {
        .it_interval = { 0 },
        .it_value = {
            .tv_sec = secs.count(),
            .tv_nsec = delay.count(),
        },
    };

    if (timer_settime(event, 0, &its, NULL) == -1) {
        LOG_CRITICAL(Service, "timer_settime failed: {}", ::strerror(errno));
    }
}

void UnscheduleTimerEvent(::timer_t event) {
    static constexpr ::itimerspec its = { 0 };

    if (timer_settime(event, 0, &its, NULL) == -1) {
        LOG_CRITICAL(Service, "timer_settime failed: {}", ::strerror(errno));
    }
}

} // namespace Service::KernelHelpers
