// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
//
// Adapted by Kent Hall for mizu on Horizon Linux.

#pragma once

#include "core/hle/service/kernel_helpers.h"
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Kernel {
class HLERequestContext;
}

namespace Service::Audio {

class IAudioIn final : public ServiceFramework<IAudioIn> {
public:
    explicit IAudioIn();
    ~IAudioIn() override;

private:
    void Start(Kernel::HLERequestContext& ctx);
    void RegisterBufferEvent(Kernel::HLERequestContext& ctx);
    void AppendAudioInBufferAuto(Kernel::HLERequestContext& ctx);

    int buffer_event;
};

class AudInU final : public ServiceFramework<AudInU> {
public:
    explicit AudInU();
    ~AudInU() override;

private:
    enum class SampleFormat : u32_le {
        PCM16 = 2,
    };

    enum class State : u32_le {
        Started = 0,
        Stopped = 1,
    };

    struct AudInOutParams {
        u32_le sample_rate{};
        u32_le channel_count{};
        SampleFormat sample_format{};
        State state{};
    };
    static_assert(sizeof(AudInOutParams) == 0x10, "AudInOutParams is an invalid size");

    using AudioInDeviceName = std::array<char, 256>;
    static constexpr std::array<std::string_view, 1> audio_device_names{{
        "BuiltInHeadset",
    }};

    void ListAudioIns(Kernel::HLERequestContext& ctx);
    void ListAudioInsAutoFiltered(Kernel::HLERequestContext& ctx);
    void OpenInOutImpl(Kernel::HLERequestContext& ctx);
    void OpenAudioIn(Kernel::HLERequestContext& ctx);
    void OpenAudioInProtocolSpecified(Kernel::HLERequestContext& ctx);
};

} // namespace Service::Audio
