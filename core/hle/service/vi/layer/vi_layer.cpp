// Copyright 2019 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
//
// Adapted by Kent Hall for mizu on Horizon Linux.

#include "core/hle/service/vi/layer/vi_layer.h"
#include "core/hle/service/nvflinger/buffer_queue.h"

namespace Service::VI {

Layer::Layer(u64 id, NVFlinger::BufferQueue& queue, ::pid_t pid)
    : layer_id{id}, buffer_queue{queue}, requester_pid{pid} {
    GrabGPU(requester_pid);
}

Layer::~Layer() {
    buffer_queue.Disconnect();
    PutGPU(requester_pid);
}

} // namespace Service::VI
