// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
//
// Adapted by Kent Hall for mizu on Horizon Linux.

#pragma once

#include "core/hle/service/service.h"

namespace Service::APM {

class Controller;
class Module;

class APM final : public ServiceFramework<APM> {
public:
    explicit APM(std::shared_ptr<Module> apm_, const char* name);
    ~APM() override;

private:
    void OpenSession(Kernel::HLERequestContext& ctx);
    void GetPerformanceMode(Kernel::HLERequestContext& ctx);
    void IsCpuOverclockEnabled(Kernel::HLERequestContext& ctx);

    std::shared_ptr<Module> apm;
};

class APM_Sys final : public ServiceFramework<APM_Sys> {
public:
    explicit APM_Sys();
    ~APM_Sys() override;

    void SetCpuBoostMode(Kernel::HLERequestContext& ctx);

private:
    void GetPerformanceEvent(Kernel::HLERequestContext& ctx);
    void GetCurrentPerformanceConfiguration(Kernel::HLERequestContext& ctx);
};

} // namespace Service::APM
