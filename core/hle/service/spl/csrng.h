// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
//
// Adapted by Kent Hall for mizu on Horizon Linux.

#pragma once

#include "core/hle/service/spl/spl_module.h"

namespace Core {
class System;
}

namespace Service::SPL {

class CSRNG final : public Module::Interface {
public:
    explicit CSRNG(Core::System& system_, std::shared_ptr<Module> module_);
    ~CSRNG() override;
};

} // namespace Service::SPL
