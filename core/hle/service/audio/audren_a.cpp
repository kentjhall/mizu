// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
//
// Adapted by Kent Hall for mizu on Horizon Linux.

#include "core/hle/service/audio/audren_a.h"

namespace Service::Audio {

AudRenA::AudRenA() : ServiceFramework{"audren:a"} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, nullptr, "RequestSuspend"},
        {1, nullptr, "RequestResume"},
        {2, nullptr, "GetProcessMasterVolume"},
        {3, nullptr, "SetProcessMasterVolume"},
        {4, nullptr, "RegisterAppletResourceUserId"},
        {5, nullptr, "UnregisterAppletResourceUserId"},
        {6, nullptr, "GetProcessRecordVolume"},
        {7, nullptr, "SetProcessRecordVolume"},
    };
    // clang-format on

    RegisterHandlers(functions);
}

AudRenA::~AudRenA() = default;

} // namespace Service::Audio
