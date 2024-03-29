// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
//
// Adapted by Kent Hall for mizu on Horizon Linux.

#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <queue>
#include <stop_token>
#include <mutex>
#include <condition_variable>

#include "audio_core/buffer.h"
#include "common/common_types.h"

namespace AudioCore {

class SinkStream;

/**
 * Represents an audio stream, which is a sequence of queued buffers, to be outputed by AudioOut
 */
class Stream {
public:
    /// Audio format of the stream
    enum class Format {
        Mono16,
        Stereo16,
        Multi51Channel16,
    };

    /// Current state of the stream
    enum class State {
        Stopped,
        Playing,
    };

    /// Callback function type, used to change guest state on a buffer being released
    using ReleaseCallback = std::function<void()>;

    Stream(u32 sample_rate_, Format format_,
           ReleaseCallback&& release_callback_, SinkStream& sink_stream_, std::string&& name_);

    ~Stream();

    /// Plays the audio stream
    void Play();

    /// Stops the audio stream
    void Stop();

    /// Queues a buffer into the audio stream, returns true on success
    bool QueueBuffer(BufferPtr&& buffer);

    /// Flush audio buffers
    bool Flush();

    /// Returns true if the audio stream contains a buffer with the specified tag
    [[nodiscard]] bool ContainsBuffer(Buffer::Tag tag) const;

    /// Returns a vector of recently released buffers specified by tag
    [[nodiscard]] std::vector<Buffer::Tag> GetTagsAndReleaseBuffers(std::size_t max_count);

    /// Returns a vector of all recently released buffers specified by tag
    [[nodiscard]] std::vector<Buffer::Tag> GetTagsAndReleaseBuffers();

    void SetVolume(float volume);

    [[nodiscard]] float GetVolume() const {
        return game_volume;
    }

    /// Returns true if the stream is currently playing
    [[nodiscard]] bool IsPlaying(bool locked = false) const {
	if (locked)
		return state == State::Playing;
        std::unique_lock lock{mutex};
        return state == State::Playing;
    }

    /// Returns the number of queued buffers
    [[nodiscard]] std::size_t GetQueueSize() const {
        return queued_buffers.size();
    }

    /// Gets the sample rate
    [[nodiscard]] u32 GetSampleRate() const {
        return sample_rate;
    }

    /// Gets the number of samples played so far
    [[nodiscard]] u64 GetPlayedSampleCount() const {
        return played_samples;
    }

    /// Gets the number of channels
    [[nodiscard]] u32 GetNumChannels() const;

    /// Get the state
    [[nodiscard]] State GetState() const;

private:
    /// Plays the next queued buffer in the audio stream, starting playback if necessary
    void PlayNextBuffer();

    /// Releases the actively playing buffer, signalling that it has been completed
    void ReleaseActiveBuffer();

    /// Gets the number of core cycles when the specified buffer will be released
    [[nodiscard]] std::chrono::nanoseconds GetBufferReleaseNS(const Buffer& buffer) const;

    u32 sample_rate;                  ///< Sample rate of the stream
    u64 played_samples{};             ///< The current played sample count
    Format format;                    ///< Format of the stream
    float game_volume = 1.0f;         ///< The volume the game currently has set
    ReleaseCallback release_callback; ///< Buffer release callback for the stream
    State state{State::Stopped};      ///< Playback state of the stream
    ::timer_t release_event;                ///< Core timing release event for the stream
    mutable std::mutex mutex;
    BufferPtr active_buffer;                ///< Actively playing buffer in the stream
    std::queue<BufferPtr> queued_buffers;   ///< Buffers queued to be played in the stream
    std::queue<BufferPtr> released_buffers; ///< Buffers recently released from the stream
    SinkStream& sink_stream;                ///< Output sink for the stream
    std::string name;                       ///< Name of the stream, must be unique

    std::stop_source stop_source;
    std::condition_variable done_cv;
    bool is_done = false;
};

using StreamPtr = std::shared_ptr<Stream>;

} // namespace AudioCore
