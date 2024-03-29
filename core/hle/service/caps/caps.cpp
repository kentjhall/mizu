// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
//
// Adapted by Kent Hall for mizu on Horizon Linux.

#include "core/hle/service/caps/caps.h"
#include "core/hle/service/caps/caps_a.h"
#include "core/hle/service/caps/caps_c.h"
#include "core/hle/service/caps/caps_sc.h"
#include "core/hle/service/caps/caps_ss.h"
#include "core/hle/service/caps/caps_su.h"
#include "core/hle/service/caps/caps_u.h"
#include "core/hle/service/service.h"

namespace Service::Capture {

void InstallInterfaces(SM::ServiceManager& sm, Core::System& system) {
    std::make_shared<CAPS_A>(system)->InstallAsService(sm);
    std::make_shared<CAPS_C>(system)->InstallAsService(sm);
    std::make_shared<CAPS_U>(system)->InstallAsService(sm);
    std::make_shared<CAPS_SC>(system)->InstallAsService(sm);
    std::make_shared<CAPS_SS>(system)->InstallAsService(sm);
    std::make_shared<CAPS_SU>(system)->InstallAsService(sm);
}

} // namespace Service::Capture
