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

class SPL final : public Module::Interface {
public:
    explicit SPL(Core::System& system_, std::shared_ptr<Module> module_);
    ~SPL() override;
};

class SPL_MIG final : public Module::Interface {
public:
    explicit SPL_MIG(Core::System& system_, std::shared_ptr<Module> module_);
    ~SPL_MIG() override;
};

class SPL_FS final : public Module::Interface {
public:
    explicit SPL_FS(Core::System& system_, std::shared_ptr<Module> module_);
    ~SPL_FS() override;
};

class SPL_SSL final : public Module::Interface {
public:
    explicit SPL_SSL(Core::System& system_, std::shared_ptr<Module> module_);
    ~SPL_SSL() override;
};

class SPL_ES final : public Module::Interface {
public:
    explicit SPL_ES(Core::System& system_, std::shared_ptr<Module> module_);
    ~SPL_ES() override;
};

class SPL_MANU final : public Module::Interface {
public:
    explicit SPL_MANU(Core::System& system_, std::shared_ptr<Module> module_);
    ~SPL_MANU() override;
};

} // namespace Service::SPL
