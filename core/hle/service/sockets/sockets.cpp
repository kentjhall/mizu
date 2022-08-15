// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
//
// Adapted by Kent Hall for mizu on Horizon Linux.

#include "core/hle/service/sm/sm.h"
#include "core/hle/service/sockets/bsd.h"
#include "core/hle/service/sockets/ethc.h"
#include "core/hle/service/sockets/nsd.h"
#include "core/hle/service/sockets/sfdnsres.h"
#include "core/hle/service/sockets/sockets.h"

namespace Service::Sockets {

void InstallInterfaces() {
    MakeService<BSD>("bsd:s");
    MakeService<BSD>("bsd:u");
    MakeService<BSDCFG>();

    MakeService<ETHC_C>();
    MakeService<ETHC_I>();

    MakeService<NSD>("nsd:a");
    MakeService<NSD>("nsd:u");

    MakeService<SFDNSRES>();
}

} // namespace Service::Sockets
