// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
//
// Adapted by Kent Hall for mizu on Horizon Linux.

#include "core/hle/service/pctl/pctl.h"

namespace Service::PCTL {

PCTL::PCTL(std::shared_ptr<Module> module_, const char* name,
           Capability capability_)
    : Interface{std::move(module_), name, capability_} {
    static const FunctionInfo functions[] = {
        {0, &PCTL::CreateService, "CreateService"},
        {1, &PCTL::CreateServiceWithoutInitialize, "CreateServiceWithoutInitialize"},
    };
    RegisterHandlers(functions);
}

PCTL::~PCTL() = default;
} // namespace Service::PCTL
