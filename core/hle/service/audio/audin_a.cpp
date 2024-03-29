// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
//
// Adapted by Kent Hall for mizu on Horizon Linux.

#include "core/hle/service/audio/audin_a.h"

namespace Service::Audio {

AudInA::AudInA() : ServiceFramework{"audin:a"} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, nullptr, "RequestSuspend"},
        {1, nullptr, "RequestResume"},
        {2, nullptr, "GetProcessMasterVolume"},
        {3, nullptr, "SetProcessMasterVolume"},
    };
    // clang-format on

    RegisterHandlers(functions);
}

AudInA::~AudInA() = default;

} // namespace Service::Audio
