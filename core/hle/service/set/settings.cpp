// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/set/set.h"
#include "core/hle/service/set/set_cal.h"
#include "core/hle/service/set/set_fd.h"
#include "core/hle/service/set/set_sys.h"
#include "core/hle/service/set/settings.h"
#include "core/hle/service/sm/sm.h"

namespace Service::Set {

void InstallInterfaces() {
    MakeService<SET>();
    MakeService<SET_CAL>();
    MakeService<SET_FD>();
    MakeService<SET_SYS>();
}

} // namespace Service::Set
