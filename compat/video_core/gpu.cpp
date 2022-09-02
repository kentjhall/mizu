// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QApplication>
#include <QDesktopWidget>

#include "common/assert.h"
#include "common/microprofile.h"
#include "core/core.h"
#include "core/memory.h"
#include "core/hle/service/service.h"
#include "video_core/engines/fermi_2d.h"
#include "video_core/engines/kepler_compute.h"
#include "video_core/engines/kepler_memory.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/engines/maxwell_dma.h"
#include "video_core/gpu.h"
#include "video_core/gpu_thread.h"
#include "video_core/memory_manager.h"
#include "video_core/renderer_base.h"
#include "video_core/bootmanager.h"

namespace Tegra {

MICROPROFILE_DEFINE(GPU_wait, "GPU", "Wait for the GPU", MP_RGB(128, 128, 192));

GPU::GPU(bool is_async, bool use_nvdec, ::pid_t session_pid)
    : is_async{is_async}, use_nvdec{use_nvdec}, session_pid{session_pid},
      title_id{Service::GetTitleID()}, perf_stats{title_id},
      gpu_thread(new VideoCommon::GPUThread::ThreadManager) {
    dma_pusher = std::make_unique<Tegra::DmaPusher>(*this);
    memory_manager = std::make_unique<Tegra::MemoryManager>();

    telemetry_session.AddInitialInfo();
    // Reset counters and set time origin to current frame
    perf_stats.GetAndResetStats(Service::GetGlobalTimeUs());
    perf_stats.BeginSystemFrame();

    render_window = std::make_unique<GRenderWindow>(*this);
    render_window->InitRenderTarget();
    render_window->show();
    render_window->setFocusPolicy(Qt::StrongFocus);

    render_window->installEventFilter(&*render_window);
    render_window->setAttribute(Qt::WA_Hover, true);

    const auto screen_geometry = QApplication::desktop()->screenGeometry(&*render_window);
    render_window->setGeometry(screen_geometry.x(), screen_geometry.y(),
                               screen_geometry.width(), screen_geometry.height() + 1);

    render_window->ShowFullscreen();
}

GPU::~GPU()
{
    const auto perf_results = perf_stats.GetAndResetStats(Service::GetGlobalTimeUs());
    constexpr auto performance = Common::Telemetry::FieldType::Performance;

    telemetry_session.AddField(performance, "Shutdown_EmulationSpeed",
                                     perf_results.emulation_speed * 100.0);
    telemetry_session.AddField(performance, "Shutdown_Framerate",
                                     perf_results.average_game_fps);
    telemetry_session.AddField(performance, "Shutdown_Frametime",
                                     perf_results.frametime * 1000.0);
    telemetry_session.AddField(performance, "Mean_Frametime_MS",
                                     perf_stats.GetMeanFrametime());
}

void GPU::BindRenderer(std::unique_ptr<VideoCore::RendererBase> renderer_)
{
    renderer = std::move(renderer_);
    auto& rasterizer{renderer->Rasterizer()};
    memory_manager->BindRasterizer(&rasterizer);
    maxwell_3d = std::make_unique<Engines::Maxwell3D>(rasterizer, *memory_manager);
    fermi_2d = std::make_unique<Engines::Fermi2D>(rasterizer);
    kepler_compute = std::make_unique<Engines::KeplerCompute>(rasterizer, *memory_manager);
    maxwell_dma = std::make_unique<Engines::MaxwellDMA>(*memory_manager);
    kepler_memory = std::make_unique<Engines::KeplerMemory>(*memory_manager);
    rasterizer.SetupDirtyFlags();
}

void GPU::RendererFrameEndNotify() {
    perf_stats.EndGameFrame();
}

void GPU::Start() {
    if (is_async)
        gpu_thread->StartThread(*renderer, *dma_pusher);
    cpu_context = renderer->GetRenderWindow().CreateSharedContext();
}

/// Obtain the CPU Context
void GPU::ObtainContext() {
    cpu_context->MakeCurrent();
}

/// Release the CPU Context
void GPU::ReleaseContext() {
    cpu_context->DoneCurrent();
}

void GPU::PushGPUEntries(Tegra::CommandList&& entries) {
    if (is_async)
        gpu_thread->SubmitList(std::move(entries));
    else {
        dma_pusher->Push(std::move(entries));
        dma_pusher->DispatchCalls();
    }
}

/// Push GPU command buffer entries to be processed
void GPU::PushCommandBuffer(Tegra::ChCommandHeaderList& entries) {
    if (!use_nvdec) {
        return;
    }

    if (!cdma_pusher) {
        cdma_pusher = std::make_unique<Tegra::CDmaPusher>(*this);
    }

    // SubmitCommandBuffer would make the nvdec operations async, this is not currently working
    // TODO(ameerj): RE proper async nvdec operation
    // gpu_thread.SubmitCommandBuffer(std::move(entries));

    cdma_pusher->ProcessEntries(std::move(entries));
}

/// Frees the CDMAPusher instance to free up resources
void GPU::ClearCdmaInstance() {
    cdma_pusher.reset();
}

void GPU::SwapBuffers(const Tegra::FramebufferConfig* framebuffer) {
    if (is_async)
        gpu_thread->SwapBuffers(framebuffer);
    else
        renderer->SwapBuffers(framebuffer);
}

void GPU::FlushRegion(CacheAddr addr, u64 size) {
    if (is_async)
        gpu_thread->FlushRegion(addr, size);
    else
        renderer->Rasterizer().FlushRegion(addr, size);
}

void GPU::InvalidateRegion(CacheAddr addr, u64 size) {
    if (is_async)
        gpu_thread->InvalidateRegion(addr, size);
    else
        renderer->Rasterizer().InvalidateRegion(addr, size);
}

void GPU::FlushAndInvalidateRegion(CacheAddr addr, u64 size) {
    if (is_async)
        gpu_thread->FlushAndInvalidateRegion(addr, size);
    else
        renderer->Rasterizer().FlushAndInvalidateRegion(addr, size);
}

void GPU::TriggerCpuInterrupt(const u32 syncpoint_id, const u32 value) const {
    if (is_async)
        Service::SharedWriter(Service::interrupt_manager)->GPUInterruptSyncpt(syncpoint_id, value);
}

void GPU::WaitIdle() const {
    if (is_async)
        gpu_thread->WaitIdle();
}

Engines::Maxwell3D& GPU::Maxwell3D() {
    return *maxwell_3d;
}

const Engines::Maxwell3D& GPU::Maxwell3D() const {
    return *maxwell_3d;
}

Engines::KeplerCompute& GPU::KeplerCompute() {
    return *kepler_compute;
}

const Engines::KeplerCompute& GPU::KeplerCompute() const {
    return *kepler_compute;
}

MemoryManager& GPU::MemoryManager() {
    return *memory_manager;
}

const MemoryManager& GPU::MemoryManager() const {
    return *memory_manager;
}

DmaPusher& GPU::DmaPusher() {
    return *dma_pusher;
}

const DmaPusher& GPU::DmaPusher() const {
    return *dma_pusher;
}

/// Returns a reference to the underlying renderer.
[[nodiscard]] VideoCore::RendererBase& GPU::Renderer() {
    return *renderer;
}

/// Returns a const reference to the underlying renderer.
[[nodiscard]] const VideoCore::RendererBase& GPU::Renderer() const {
    return *renderer;
}

void GPU::WaitFence(u32 syncpoint_id, u32 value) {
    // Synced GPU, is always in sync
    if (!is_async) {
        return;
    }
    if (syncpoint_id == UINT32_MAX) {
        // TODO: Research what this does.
        LOG_ERROR(HW_GPU, "Waiting for syncpoint -1 not implemented");
        return;
    }
    MICROPROFILE_SCOPE(GPU_wait);
    std::unique_lock lock{sync_mutex};
    sync_cv.wait(lock, [=, this]() {
        if (shutting_down) {
            // We're shutting down, ensure no threads continue to wait for the next syncpoint
            return true;
        }
        return syncpoints[syncpoint_id].load() >= value;
    });
}

void GPU::IncrementSyncPoint(const u32 syncpoint_id) {
    syncpoints[syncpoint_id]++;
    std::lock_guard lock{sync_mutex};
    sync_cv.notify_all();
    if (!syncpt_interrupts[syncpoint_id].empty()) {
        u32 value = syncpoints[syncpoint_id].load();
        auto it = syncpt_interrupts[syncpoint_id].begin();
        while (it != syncpt_interrupts[syncpoint_id].end()) {
            if (value >= *it) {
                TriggerCpuInterrupt(syncpoint_id, *it);
                it = syncpt_interrupts[syncpoint_id].erase(it);
                continue;
            }
            it++;
        }
    }
}

u32 GPU::GetSyncpointValue(const u32 syncpoint_id) const {
    return syncpoints[syncpoint_id].load();
}

void GPU::RegisterSyncptInterrupt(const u32 syncpoint_id, const u32 value) {
    auto& interrupt = syncpt_interrupts[syncpoint_id];
    bool contains = std::any_of(interrupt.begin(), interrupt.end(),
                                [value](u32 in_value) { return in_value == value; });
    if (contains) {
        return;
    }
    syncpt_interrupts[syncpoint_id].emplace_back(value);
}

bool GPU::CancelSyncptInterrupt(const u32 syncpoint_id, const u32 value) {
    std::lock_guard lock{sync_mutex};
    auto& interrupt = syncpt_interrupts[syncpoint_id];
    const auto iter =
        std::find_if(interrupt.begin(), interrupt.end(),
                     [value](u32 interrupt_value) { return value == interrupt_value; });

    if (iter == interrupt.end()) {
        return false;
    }
    interrupt.erase(iter);
    return true;
}

u64 GPU::GetTicks() const {
    // This values were reversed engineered by fincs from NVN
    // The gpu clock is reported in units of 385/625 nanoseconds
    constexpr u64 gpu_ticks_num = 384;
    constexpr u64 gpu_ticks_den = 625;

    const u64 nanoseconds = Service::GetGlobalTimeNs().count();
    const u64 nanoseconds_num = nanoseconds / gpu_ticks_den;
    const u64 nanoseconds_rem = nanoseconds % gpu_ticks_den;
    return nanoseconds_num * gpu_ticks_num + (nanoseconds_rem * gpu_ticks_num) / gpu_ticks_den;
}

void GPU::NotifySessionClose() {
    std::unique_lock lock{sync_mutex};
    shutting_down = true;
    sync_cv.notify_all();
}

void GPU::FlushCommands() {
    renderer->Rasterizer().FlushCommands();
}

/// Synchronizes CPU writes with Host GPU memory.
void GPU::SyncGuestHost() {
    renderer->Rasterizer().SyncGuestHost();
}

enum class GpuSemaphoreOperation {
    AcquireEqual = 0x1,
    WriteLong = 0x2,
    AcquireGequal = 0x4,
    AcquireMask = 0x8,
};

void GPU::CallMethod(const MethodCall& method_call) {
    LOG_TRACE(HW_GPU, "Processing method {:08X} on subchannel {}", method_call.method,
              method_call.subchannel);

    ASSERT(method_call.subchannel < bound_engines.size());

    if (ExecuteMethodOnEngine(method_call)) {
        CallEngineMethod(method_call);
    } else {
        CallPullerMethod(method_call);
    }
}

bool GPU::ExecuteMethodOnEngine(const MethodCall& method_call) {
    const auto method = static_cast<BufferMethods>(method_call.method);
    return method >= BufferMethods::NonPullerMethods;
}

void GPU::CallPullerMethod(const MethodCall& method_call) {
    regs.reg_array[method_call.method] = method_call.argument;
    const auto method = static_cast<BufferMethods>(method_call.method);

    switch (method) {
    case BufferMethods::BindObject: {
        ProcessBindMethod(method_call);
        break;
    }
    case BufferMethods::Nop:
    case BufferMethods::SemaphoreAddressHigh:
    case BufferMethods::SemaphoreAddressLow:
    case BufferMethods::SemaphoreSequence:
    case BufferMethods::RefCnt:
    case BufferMethods::UnkCacheFlush:
    case BufferMethods::WrcacheFlush:
    case BufferMethods::FenceValue:
        break;
    case BufferMethods::FenceAction:
        ProcessFenceActionMethod();
        break;
    case BufferMethods::SemaphoreTrigger: {
        ProcessSemaphoreTriggerMethod();
        break;
    }
    case BufferMethods::NotifyIntr: {
        // TODO(Kmather73): Research and implement this method.
        LOG_ERROR(HW_GPU, "Special puller engine method NotifyIntr not implemented");
        break;
    }
    case BufferMethods::Unk28: {
        // TODO(Kmather73): Research and implement this method.
        LOG_ERROR(HW_GPU, "Special puller engine method Unk28 not implemented");
        break;
    }
    case BufferMethods::SemaphoreAcquire: {
        ProcessSemaphoreAcquire();
        break;
    }
    case BufferMethods::SemaphoreRelease: {
        ProcessSemaphoreRelease();
        break;
    }
    case BufferMethods::Yield: {
        // TODO(Kmather73): Research and implement this method.
        LOG_ERROR(HW_GPU, "Special puller engine method Yield not implemented");
        break;
    }
    default:
        LOG_ERROR(HW_GPU, "Special puller engine method {:X} not implemented",
                  static_cast<u32>(method));
        break;
    }
}

void GPU::CallEngineMethod(const MethodCall& method_call) {
    const EngineID engine = bound_engines[method_call.subchannel];

    switch (engine) {
    case EngineID::FERMI_TWOD_A:
        fermi_2d->CallMethod(method_call);
        break;
    case EngineID::MAXWELL_B:
        maxwell_3d->CallMethod(method_call);
        break;
    case EngineID::KEPLER_COMPUTE_B:
        kepler_compute->CallMethod(method_call);
        break;
    case EngineID::MAXWELL_DMA_COPY_A:
        maxwell_dma->CallMethod(method_call);
        break;
    case EngineID::KEPLER_INLINE_TO_MEMORY_B:
        kepler_memory->CallMethod(method_call);
        break;
    default:
        UNIMPLEMENTED_MSG("Unimplemented engine");
    }
}

void GPU::ProcessBindMethod(const MethodCall& method_call) {
    // Bind the current subchannel to the desired engine id.
    LOG_DEBUG(HW_GPU, "Binding subchannel {} to engine {}", method_call.subchannel,
              method_call.argument);
    bound_engines[method_call.subchannel] = static_cast<EngineID>(method_call.argument);
}

void GPU::ProcessFenceActionMethod() {
    switch (regs.fence_action.op) {
    case GPU::FenceOperation::Acquire:
        WaitFence(regs.fence_action.syncpoint_id, regs.fence_value);
        break;
    case GPU::FenceOperation::Increment:
        IncrementSyncPoint(regs.fence_action.syncpoint_id);
        break;
    default:
        UNIMPLEMENTED_MSG("Unimplemented operation {}", regs.fence_action.op.Value());
    }
}

void GPU::ProcessSemaphoreTriggerMethod() {
    const auto semaphoreOperationMask = 0xF;
    const auto op =
        static_cast<GpuSemaphoreOperation>(regs.semaphore_trigger & semaphoreOperationMask);
    if (op == GpuSemaphoreOperation::WriteLong) {
        struct Block {
            u32 sequence;
            u32 zeros = 0;
            u64 timestamp;
        };

        Block block{};
        block.sequence = regs.semaphore_sequence;
        // TODO(Kmather73): Generate a real GPU timestamp and write it here instead of
        // CoreTiming
        block.timestamp = GetTicks();
        memory_manager->WriteBlock(regs.semaphore_address.SemaphoreAddress(), &block,
                                   sizeof(block));
    } else {
        const u32 word{memory_manager->Read<u32>(regs.semaphore_address.SemaphoreAddress())};
        if ((op == GpuSemaphoreOperation::AcquireEqual && word == regs.semaphore_sequence) ||
            (op == GpuSemaphoreOperation::AcquireGequal &&
             static_cast<s32>(word - regs.semaphore_sequence) > 0) ||
            (op == GpuSemaphoreOperation::AcquireMask && (word & regs.semaphore_sequence))) {
            // Nothing to do in this case
        } else {
            regs.acquire_source = true;
            regs.acquire_value = regs.semaphore_sequence;
            if (op == GpuSemaphoreOperation::AcquireEqual) {
                regs.acquire_active = true;
                regs.acquire_mode = false;
            } else if (op == GpuSemaphoreOperation::AcquireGequal) {
                regs.acquire_active = true;
                regs.acquire_mode = true;
            } else if (op == GpuSemaphoreOperation::AcquireMask) {
                // TODO(kemathe) The acquire mask operation waits for a value that, ANDed with
                // semaphore_sequence, gives a non-0 result
                LOG_ERROR(HW_GPU, "Invalid semaphore operation AcquireMask not implemented");
            } else {
                LOG_ERROR(HW_GPU, "Invalid semaphore operation");
            }
        }
    }
}

void GPU::ProcessSemaphoreRelease() {
    memory_manager->Write<u32>(regs.semaphore_address.SemaphoreAddress(), regs.semaphore_release);
}

void GPU::ProcessSemaphoreAcquire() {
    const u32 word = memory_manager->Read<u32>(regs.semaphore_address.SemaphoreAddress());
    const auto value = regs.semaphore_acquire;
    if (word != value) {
        regs.acquire_active = true;
        regs.acquire_value = value;
        // TODO(kemathe73) figure out how to do the acquire_timeout
        regs.acquire_mode = false;
        regs.acquire_source = false;
    }
}

Core::TelemetrySession& GPU::TelemetrySession() {
    return telemetry_session;
};

const Core::TelemetrySession& GPU::TelemetrySession() const {
    return telemetry_session;
};

Core::PerfStats& GPU::GetPerfStats() {
    return perf_stats;
};

const Core::PerfStats& GPU::GetPerfStats() const {
    return perf_stats;
};

Core::SpeedLimiter& GPU::SpeedLimiter() {
    return speed_limiter;
};

const Core::SpeedLimiter& GPU::SpeedLimiter() const {
    return speed_limiter;
};

GRenderWindow& GPU::RenderWindow() {
    return *render_window;
}

const GRenderWindow& GPU::RenderWindow() const {
    return *render_window;
}

::pid_t GPU::SessionPid() const {
    return session_pid;
}

u64 GPU::TitleId() const {
    return title_id;
}

} // namespace Tegra
