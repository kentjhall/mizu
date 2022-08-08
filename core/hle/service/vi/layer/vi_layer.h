// Copyright 2019 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
//
// Adapted by Kent Hall for mizu on Horizon Linux.

#pragma once

#include <sys/types.h>
#include "common/common_types.h"
#include "core/hle/service/service.h"
#include "video_core/gpu.h"

namespace Service::NVFlinger {
class BufferQueue;
}

namespace Service::VI {

/// Represents a single display layer.
class Layer {
public:
    /// Constructs a layer with a given ID and buffer queue.
    ///
    /// @param id    The ID to assign to this layer.
    /// @param queue The buffer queue for this layer to use.
    /// @param pid   The PID of the requesting thread.
    ///
    Layer(u64 id, NVFlinger::BufferQueue& queue, ::pid_t pid);
    ~Layer();

    Layer(const Layer&) = delete;
    Layer& operator=(const Layer&) = delete;

    Layer(Layer&&) = default;
    Layer& operator=(Layer&&) = delete;

    /// Gets the id for this layer.
    u64 GetID() const {
        return layer_id;
    }

    /// Gets a reference to the buffer queue this layer is using.
    NVFlinger::BufferQueue& GetBufferQueue() {
        return buffer_queue;
    }

    /// Gets the GPU for this layer.
    Shared<Tegra::GPU>& GPU() {
        return Service::GPU(requester_pid);
    }

    ::pid_t GetRequesterPid() {
	    return requester_pid;
    }

    /// Gets a const reference to the buffer queue this layer is using.
    const NVFlinger::BufferQueue& GetBufferQueue() const {
        return buffer_queue;
    }

private:
    u64 layer_id;
    NVFlinger::BufferQueue& buffer_queue;
    ::pid_t requester_pid;
};

} // namespace Service::VI
