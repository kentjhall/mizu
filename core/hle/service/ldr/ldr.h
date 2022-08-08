// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
//
// Adapted by Kent Hall for mizu on Horizon Linux.

#pragma once

namespace Core {
class System;
}

namespace Service::SM {
class ServiceManager;
}

namespace Service::LDR {

/// Registers all LDR services with the specified service manager.
void InstallInterfaces(SM::ServiceManager& sm, Core::System& system);

} // namespace Service::LDR
