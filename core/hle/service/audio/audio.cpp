// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
//
// Adapted by Kent Hall for mizu on Horizon Linux.

#include "core/hle/service/audio/audctl.h"
#include "core/hle/service/audio/auddbg.h"
#include "core/hle/service/audio/audin_a.h"
#include "core/hle/service/audio/audin_u.h"
#include "core/hle/service/audio/audio.h"
#include "core/hle/service/audio/audout_a.h"
#include "core/hle/service/audio/audout_u.h"
#include "core/hle/service/audio/audrec_a.h"
#include "core/hle/service/audio/audrec_u.h"
#include "core/hle/service/audio/audren_a.h"
#include "core/hle/service/audio/audren_u.h"
#include "core/hle/service/audio/codecctl.h"
#include "core/hle/service/audio/hwopus.h"
#include "core/hle/service/sm/sm.h"
#include "core/hle/service/service.h"

namespace Service::Audio {

void InstallInterfaces() {
    MakeService<AudCtl>();
    MakeService<AudOutA>();
    MakeService<AudOutU>();
    MakeService<AudInA>();
    MakeService<AudInU>();
    MakeService<AudRecA>();
    MakeService<AudRecU>();
    MakeService<AudRenA>();
    MakeService<AudRenU>();
    MakeService<CodecCtl>();
    MakeService<HwOpus>();

    MakeService<AudDbg>("audin:d");
    MakeService<AudDbg>("audout:d");
    MakeService<AudDbg>("audrec:d");
    MakeService<AudDbg>("audren:d");
}

} // namespace Service::Audio
