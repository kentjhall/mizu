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

namespace Service::Audio {

class AudDbg final : public ServiceFramework<AudDbg> {
public:
    explicit AudDbg(const char* name);
    ~AudDbg() override;
};

} // namespace Service::Audio
