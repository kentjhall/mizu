// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
//
// Adapted by Kent Hall for mizu on Horizon Linux.

#pragma once

#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service::Glue {

class ECTX_AW final : public ServiceFramework<ECTX_AW> {
public:
    explicit ECTX_AW();
    ~ECTX_AW() override;
};

} // namespace Service::Glue
