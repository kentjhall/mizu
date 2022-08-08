// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
//
// Adapted by Kent Hall for mizu on Horizon Linux.

#pragma once

#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Vulkan {

vk::DebugUtilsMessenger CreateDebugCallback(const vk::Instance& instance);

} // namespace Vulkan
