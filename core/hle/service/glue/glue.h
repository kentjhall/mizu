// Copyright 2019 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
//
// Adapted by Kent Hall for mizu on Horizon Linux.

#pragma once

namespace Core {
class System;
} // namespace Core

namespace Service::Glue {

/// Registers all Glue services with the specified service manager.
void InstallInterfaces();

} // namespace Service::Glue
