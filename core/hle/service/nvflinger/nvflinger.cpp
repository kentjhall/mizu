// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <optional>

#include "common/assert.h"
#include "common/logging/log.h"
#include "common/microprofile.h"
#include "common/scope_exit.h"
#include "common/settings.h"
#include "common/thread.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/hle/kernel/k_readable_event.h"
#include "core/hle/service/nvdrv/devices/nvdisp_disp0.h"
#include "core/hle/service/nvdrv/nvdrv.h"
#include "core/hle/service/nvflinger/buffer_queue.h"
#include "core/hle/service/nvflinger/nvflinger.h"
#include "core/hle/service/vi/display/vi_display.h"
#include "core/hle/service/vi/layer/vi_layer.h"
#include "video_core/gpu.h"

namespace Service::NVFlinger {

constexpr auto frame_ns = std::chrono::nanoseconds{1000000000 / 60};

void NVFlinger::SplitVSync(std::stop_token stop_token) {
    std::string name = "mizu:VSyncThread";
    MicroProfileOnThreadCreate(name.c_str());

    // Cleanup
    SCOPE_EXIT({ MicroProfileOnThreadExit(); });

    Common::SetCurrentThreadName(name.c_str());
    Common::SetCurrentThreadPriority(Common::ThreadPriority::High);
    s64 delay = 0;
    while (!stop_token.stop_requested()) {
        guard->lock();
        const s64 time_start = GetGlobalTimeNs().count();
        Compose();
        const auto ticks = GetNextTicks();
        const s64 time_end = GetGlobalTimeNs().count();
        const s64 time_passed = time_end - time_start;
        const s64 next_time = std::max<s64>(0, ticks - time_passed - delay);
        guard->unlock();
        if (next_time > 0) {
            std::this_thread::sleep_for(std::chrono::nanoseconds{next_time});
        }
        delay = (GetGlobalTimeNs().count() - time_end) - next_time;
    }
}

NVFlinger::NVFlinger() {
    displays.emplace_back(0, "Default");
    displays.emplace_back(1, "External");
    displays.emplace_back(2, "Edid");
    displays.emplace_back(3, "Internal");
    displays.emplace_back(4, "Null");
    guard = std::make_shared<std::mutex>();

    KernelHelpers::SetupServiceContext("nvflinger");

    // Schedule the screen composition events
    composition_event = KernelHelpers::CreateTimerEvent(
        "ScreenComposition",
        this,
        [](::sigval sigev_value) {
            auto nvf = static_cast<NVFlinger *>(sigev_value.sival_ptr);
            const auto lock_guard = nvf->Lock();
            nvf->Compose();

            const auto future_ns = std::chrono::nanoseconds{nvf->GetNextTicks()};

            KernelHelpers::ScheduleTimerEvent(future_ns, nvf->composition_event);
        });

    if (Settings::values.use_multi_core) {
        vsync_thread = std::jthread([this](std::stop_token token) { SplitVSync(token); });
    } else {
        KernelHelpers::ScheduleTimerEvent(frame_ns, composition_event);
    }
}

NVFlinger::~NVFlinger() {
    for (auto& buffer_queue : buffer_queues) {
        buffer_queue->Disconnect();
    }
    if (!Settings::values.use_multi_core) {
        KernelHelpers::UnscheduleTimerEvent(composition_event);
    }
}

void NVFlinger::SetNVDrvInstance(std::shared_ptr<Nvidia::Module> instance) {
    nvdrv = std::move(instance);
}

std::optional<u64> NVFlinger::OpenDisplay(std::string_view name) {
    const auto lock_guard = Lock();

    LOG_DEBUG(Service, "Opening \"{}\" display", name);

    // TODO(Subv): Currently we only support the Default display.
    ASSERT(name == "Default");

    const auto itr =
        std::find_if(displays.begin(), displays.end(),
                     [&](const VI::Display& display) { return display.GetName() == name; });

    if (itr == displays.end()) {
        return std::nullopt;
    }

    return itr->GetID();
}

std::optional<u64> NVFlinger::CreateLayer(u64 display_id) {
    const auto lock_guard = Lock();
    auto* const display = FindDisplay(display_id);

    if (display == nullptr) {
        return std::nullopt;
    }

    const u64 layer_id = next_layer_id++;
    CreateLayerAtId(*display, layer_id);
    return layer_id;
}

void NVFlinger::CreateLayerAtId(VI::Display& display, u64 layer_id) {
    const u32 buffer_queue_id = next_buffer_queue_id++;
    buffer_queues.emplace_back(
        std::make_unique<BufferQueue>(buffer_queue_id, layer_id));
    display.CreateLayer(layer_id, *buffer_queues.back());
}

void NVFlinger::CloseLayer(u64 layer_id) {
    const auto lock_guard = Lock();

    for (auto& display : displays) {
        display.CloseLayer(layer_id);
    }
}

std::optional<u32> NVFlinger::FindBufferQueueId(u64 display_id, u64 layer_id) {
    const auto lock_guard = Lock();
    const auto* const layer = FindOrCreateLayer(display_id, layer_id);

    if (layer == nullptr) {
        return std::nullopt;
    }

    return layer->GetBufferQueue().GetId();
}

int NVFlinger::FindVsyncEvent(u64 display_id) {
    const auto lock_guard = Lock();
    auto* const display = FindDisplay(display_id);

    if (display == nullptr) {
        return -1;
    }

    return display->GetVSyncEvent();
}

BufferQueue* NVFlinger::FindBufferQueue(u32 id) {
    const auto lock_guard = Lock();
    const auto itr = std::find_if(buffer_queues.begin(), buffer_queues.end(),
                                  [id](const auto& queue) { return queue->GetId() == id; });

    if (itr == buffer_queues.end()) {
        return nullptr;
    }

    return itr->get();
}

VI::Display* NVFlinger::FindDisplay(u64 display_id) {
    const auto itr =
        std::find_if(displays.begin(), displays.end(),
                     [&](const VI::Display& display) { return display.GetID() == display_id; });

    if (itr == displays.end()) {
        return nullptr;
    }

    return &*itr;
}

const VI::Display* NVFlinger::FindDisplay(u64 display_id) const {
    const auto itr =
        std::find_if(displays.begin(), displays.end(),
                     [&](const VI::Display& display) { return display.GetID() == display_id; });

    if (itr == displays.end()) {
        return nullptr;
    }

    return &*itr;
}

VI::Layer* NVFlinger::FindLayer(u64 display_id, u64 layer_id) {
    auto* const display = FindDisplay(display_id);

    if (display == nullptr) {
        return nullptr;
    }

    return display->FindLayer(layer_id);
}

const VI::Layer* NVFlinger::FindLayer(u64 display_id, u64 layer_id) const {
    const auto* const display = FindDisplay(display_id);

    if (display == nullptr) {
        return nullptr;
    }

    return display->FindLayer(layer_id);
}

VI::Layer* NVFlinger::FindOrCreateLayer(u64 display_id, u64 layer_id) {
    auto* const display = FindDisplay(display_id);

    if (display == nullptr) {
        return nullptr;
    }

    auto* layer = display->FindLayer(layer_id);

    if (layer == nullptr) {
        LOG_DEBUG(Service, "Layer at id {} not found. Trying to create it.", layer_id);
        CreateLayerAtId(*display, layer_id);
        return display->FindLayer(layer_id);
    }

    return layer;
}

void NVFlinger::Compose() {
    for (auto& display : displays) {
        // Trigger vsync for this display at the end of drawing
        SCOPE_EXIT({ display.SignalVSyncEvent(); });

        // Don't do anything for displays without layers.
        if (!display.HasLayers())
            continue;

        // TODO(Subv): Support more than 1 layer.
        VI::Layer& layer = display.GetLayer(0);
        auto& buffer_queue = layer.GetBufferQueue();

        // Search for a queued buffer and acquire it
        auto buffer = buffer_queue.AcquireBuffer();

        if (!buffer) {
            continue;
        }

        const auto& igbp_buffer = buffer->get().igbp_buffer;

        auto& gpu = system.GPU();
        const auto& multi_fence = buffer->get().multi_fence;
        guard->unlock();
        for (u32 fence_id = 0; fence_id < multi_fence.num_fences; fence_id++) {
            const auto& fence = multi_fence.fences[fence_id];
            gpu.WaitFence(fence.id, fence.value);
        }
        guard->lock();

        MicroProfileFlip();

        // Now send the buffer to the GPU for drawing.
        // TODO(Subv): Support more than just disp0. The display device selection is probably based
        // on which display we're drawing (Default, Internal, External, etc)
        auto nvdisp = nvdrv->GetDevice<Nvidia::Devices::nvdisp_disp0>("/dev/nvdisp_disp0");
        ASSERT(nvdisp);

        nvdisp->flip(igbp_buffer.gpu_buffer_id, igbp_buffer.offset, igbp_buffer.external_format,
                     igbp_buffer.width, igbp_buffer.height, igbp_buffer.stride,
                     buffer->get().transform, buffer->get().crop_rect);

        swap_interval = buffer->get().swap_interval;
        buffer_queue.ReleaseBuffer(buffer->get().slot);
    }
}

s64 NVFlinger::GetNextTicks() const {
    static constexpr s64 max_hertz = 120LL;

    const auto& settings = Settings::values;
    const bool unlocked_fps = settings.disable_fps_limit.GetValue();
    const s64 fps_cap = unlocked_fps ? static_cast<s64>(settings.fps_cap.GetValue()) : 1;
    return (1000000000 * (1LL << swap_interval)) / (max_hertz * fps_cap);
}

} // namespace Service::NVFlinger