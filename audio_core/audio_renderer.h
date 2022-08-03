// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <memory>
#include <mutex>
#include <vector>
#include <stop_token>
#include <condition_variable>

#include "audio_core/behavior_info.h"
#include "audio_core/command_generator.h"
#include "audio_core/common.h"
#include "audio_core/effect_context.h"
#include "audio_core/memory_pool.h"
#include "audio_core/mix_context.h"
#include "audio_core/sink_context.h"
#include "audio_core/splitter_context.h"
#include "audio_core/stream.h"
#include "audio_core/voice_context.h"
#include "common/common_funcs.h"
#include "common/common_types.h"
#include "common/swap.h"
#include "core/hle/result.h"

namespace AudioCore {
using DSPStateHolder = std::array<VoiceState*, AudioCommon::MAX_CHANNEL_COUNT>;

class AudioOut;

class AudioRenderer {
public:
    AudioRenderer(AudioCommon::AudioRendererParameter params,
                  Stream::ReleaseCallback&& release_callback, std::size_t instance_number,
                  ::pid_t pid);
    ~AudioRenderer();

    [[nodiscard]] ResultCode UpdateAudioRenderer(const std::vector<u8>& input_params,
                                                 std::vector<u8>& output_params);
    [[nodiscard]] ResultCode Start();
    [[nodiscard]] ResultCode Stop();
    void QueueMixedBuffer(Buffer::Tag tag);
    void ReleaseAndQueueBuffers();
    [[nodiscard]] u32 GetSampleRate() const;
    [[nodiscard]] u32 GetSampleCount() const;
    [[nodiscard]] u32 GetMixBufferCount() const;
    [[nodiscard]] Stream::State GetStreamState() const;

private:
    BehaviorInfo behavior_info{};

    AudioCommon::AudioRendererParameter worker_params;
    std::vector<ServerMemoryPoolInfo> memory_pool_info;
    VoiceContext voice_context;
    EffectContext effect_context;
    MixContext mix_context;
    SinkContext sink_context;
    SplitterContext splitter_context;
    std::vector<VoiceState> voices;
    std::unique_ptr<AudioOut> audio_out;
    StreamPtr stream;
    CommandGenerator command_generator;
    std::size_t elapsed_frame_count{};
    ::timer_t process_event;
    std::mutex mutex;
    std::stop_source stop_source;
    std::condition_variable done_cv;
    bool is_done = false;
};

} // namespace AudioCore
