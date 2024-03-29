// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
//
// Adapted by Kent Hall for mizu on Horizon Linux.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include "audio_core/time_stretch.h"
#include "common/logging/log.h"

namespace AudioCore {

TimeStretcher::TimeStretcher(u32 sample_rate, u32 channel_count) : m_sample_rate{sample_rate} {
    m_sound_touch.setChannels(channel_count);
    m_sound_touch.setSampleRate(sample_rate);
    m_sound_touch.setPitch(1.0);
    m_sound_touch.setTempo(1.0);
}

void TimeStretcher::Clear() {
    m_sound_touch.clear();
}

void TimeStretcher::Flush() {
    m_sound_touch.flush();
}

std::size_t TimeStretcher::Process(const s16* in, std::size_t num_in, s16* out,
                                   std::size_t num_out) {
    const double time_delta = static_cast<double>(num_out) / m_sample_rate; // seconds

    // We were given actual_samples number of samples, and num_samples were requested from us.
    double current_ratio = static_cast<double>(num_in) / static_cast<double>(num_out);

    const double max_latency = 0.25; // seconds
    const double max_backlog = m_sample_rate * max_latency;
    const double backlog_fullness = m_sound_touch.numSamples() / max_backlog;
    if (backlog_fullness > 4.0) {
        // Too many samples in backlog: Don't push anymore on
        num_in = 0;
    }

    // We ideally want the backlog to be about 50% full.
    // This gives some headroom both ways to prevent underflow and overflow.
    // We tweak current_ratio to encourage this.
    constexpr double tweak_time_scale = 0.05; // seconds
    const double tweak_correction = (backlog_fullness - 0.5) * (time_delta / tweak_time_scale);
    current_ratio *= std::pow(1.0 + 2.0 * tweak_correction, tweak_correction < 0 ? 3.0 : 1.0);

    // This low-pass filter smoothes out variance in the calculated stretch ratio.
    // The time-scale determines how responsive this filter is.
    constexpr double lpf_time_scale = 0.712; // seconds
    const double lpf_gain = 1.0 - std::exp(-time_delta / lpf_time_scale);
    m_stretch_ratio += lpf_gain * (current_ratio - m_stretch_ratio);

    // Place a lower limit of 5% speed. When a game boots up, there will be
    // many silence samples. These do not need to be timestretched.
    m_stretch_ratio = std::max(m_stretch_ratio, 0.05);
    m_sound_touch.setTempo(m_stretch_ratio);

    LOG_TRACE(Audio, "{:5}/{:5} ratio:{:0.6f} backlog:{:0.6f}", num_in, num_out, m_stretch_ratio,
              backlog_fullness);

    m_sound_touch.putSamples(in, static_cast<u32>(num_in));
    return m_sound_touch.receiveSamples(out, static_cast<u32>(num_out));
}

} // namespace AudioCore
