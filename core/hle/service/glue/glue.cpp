// Copyright 2019 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
//
// Adapted by Kent Hall for mizu on Horizon Linux.

#include <memory>
#include "core/core.h"
#include "core/hle/service/sm/sm.h"
#include "core/hle/service/glue/arp.h"
#include "core/hle/service/glue/bgtc.h"
#include "core/hle/service/glue/ectx.h"
#include "core/hle/service/glue/glue.h"

namespace Service::Glue {

void InstallInterfaces() {
    // ARP
    MakeService<ARP_R>();
    MakeService<ARP_W>();

    // BackGround Task Controller
    MakeService<BGTC_T>();
    MakeService<BGTC_SC>();

    // Error Context
    MakeService<ECTX_AW>();
}

} // namespace Service::Glue
