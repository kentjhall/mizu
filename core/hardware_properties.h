// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
//
// Adapted by Kent Hall for mizu on Horizon Linux.

#pragma once

#include <array>
#include <tuple>
#include <ctime>
#include <unistd.h>

#include "common/bit_util.h"
#include "common/common_types.h"

namespace Core {

namespace Hardware {

constexpr u64 CNTFREQ = CLOCKS_PER_SEC;

} // namespace Hardware

} // namespace Core
