// Copyright 2021 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <ctime>
#include <csignal>

namespace Service::KernelHelpers {

void SetupServiceContext(std::string name_);

int CreateEvent(std::string&& name);

void CloseEvent(int);

void SignalEvent(int);

void ClearEvent(int);

::timer_t CreateTimerEvent(std::string&& name, void *val, void (*cb)(::sigval));

void CloseTimerEvent(::timer_t event);

void ScheduleTimerEvent(std::chrono::nanoseconds interval, ::timer_t event);

void ScheduleRepeatTimerEvent(std::chrono::nanoseconds interval, ::timer_t event);

void UnscheduleTimerEvent(::timer_t event);

} // namespace Service::KernelHelpers
