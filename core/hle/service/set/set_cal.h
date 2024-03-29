// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
//
// Adapted by Kent Hall for mizu on Horizon Linux.

#pragma once

#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service::Set {

class SET_CAL final : public ServiceFramework<SET_CAL> {
public:
    explicit SET_CAL();
    ~SET_CAL() override;
};

} // namespace Service::Set
