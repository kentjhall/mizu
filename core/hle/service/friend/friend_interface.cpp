// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
//
// Adapted by Kent Hall for mizu on Horizon Linux.

#include "core/hle/service/friend/friend_interface.h"

namespace Service::Friend {

Friend::Friend(std::shared_ptr<Module> module_, const char* name)
    : Interface(std::move(module_), name) {
    static const FunctionInfo functions[] = {
        {0, &Friend::CreateFriendService, "CreateFriendService"},
        {1, &Friend::CreateNotificationService, "CreateNotificationService"},
        {2, nullptr, "CreateDaemonSuspendSessionService"},
    };
    RegisterHandlers(functions);
}

Friend::~Friend() = default;

} // namespace Service::Friend
