// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
//
// Adapted by Kent Hall for mizu on Horizon Linux.

#pragma once

#include "core/hle/service/service.h"

namespace Kernel {
class HLERequestContext;
}

namespace Service::NVFlinger {
class NVFlinger;
}

namespace Service::VI {

class VI_M final : public ServiceFramework<VI_M> {
public:
    explicit VI_M();
    ~VI_M() override;

private:
    void GetDisplayService(Kernel::HLERequestContext& ctx);
};

} // namespace Service::VI
