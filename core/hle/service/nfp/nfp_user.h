// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
//
// Adapted by Kent Hall for mizu on Horizon Linux.

#pragma once

#include "core/hle/service/nfp/nfp.h"

namespace Service::NFP {

class NFP_User final : public Module::Interface {
public:
    explicit NFP_User();
    ~NFP_User() override;
};

} // namespace Service::NFP
