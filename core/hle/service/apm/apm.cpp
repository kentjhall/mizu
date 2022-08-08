// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
//
// Adapted by Kent Hall for mizu on Horizon Linux.

#include "core/core.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/service/sm/sm.h"
#include "core/hle/service/apm/apm.h"
#include "core/hle/service/apm/apm_interface.h"

namespace Service::APM {

Module::Module() = default;
Module::~Module() = default;

void InstallInterfaces() {
    auto module_ = std::make_shared<Module>();
    MakeService<APM>(module_, "apm");
    MakeService<APM>(module_, "apm:p");
    MakeService<APM>(module_, "apm:am");
    MakeService<APM_Sys>();
}

} // namespace Service::APM
