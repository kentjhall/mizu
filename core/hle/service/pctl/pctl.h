// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
//
// Adapted by Kent Hall for mizu on Horizon Linux.

#pragma once

#include "core/hle/service/pctl/pctl_module.h"

namespace Core {
class System;
}

namespace Service::PCTL {

class PCTL final : public Module::Interface {
public:
    explicit PCTL(std::shared_ptr<Module> module_, const char* name,
                  Capability capability_);
    ~PCTL() override;
};

} // namespace Service::PCTL
