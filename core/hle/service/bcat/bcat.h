// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/bcat/bcat_module.h"

namespace Core {
class System;
}

namespace Service::BCAT {

class BCAT final : public Module::Interface {
public:
    explicit BCAT(std::shared_ptr<Module> module_, const char* name_);
    ~BCAT() override;
};

} // namespace Service::BCAT
