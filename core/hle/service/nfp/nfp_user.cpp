// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
//
// Adapted by Kent Hall for mizu on Horizon Linux.

#include "core/hle/service/nfp/nfp_user.h"

namespace Service::NFP {

NFP_User::NFP_User()
    : Interface("nfp:user") {
    static const FunctionInfo functions[] = {
        {0, &NFP_User::CreateUserInterface, "CreateUserInterface"},
    };
    RegisterHandlers(functions);
}

NFP_User::~NFP_User() = default;

} // namespace Service::NFP
