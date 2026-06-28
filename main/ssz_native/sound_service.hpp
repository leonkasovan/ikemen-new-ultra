#pragma once

// sound_service.hpp — Native equivalent of ssz_script/lib/sound.ssz
//
// Provides:
// - RAII wrapper for audio Client object
// - start, stop, bufferReady, setBuffer
//
// Design note: sound_service delegates to the SSZ native plugin layer
// (main/sound/sound.cpp). Audio operations depend on platform-specific
// audio backends managed by the native plugin (PortAudio/SDL_mixer).
// Using the call-through pattern (mirroring file_service and socket_service)
// avoids duplicating audio device initialization and buffer management.

#include <cstdint>

namespace ikemen::ssz_native::sound {

// Audio format constants matching SSZ
inline constexpr int FREQ = 48000;
inline constexpr int CHANNELS = 2;
inline constexpr int BUFFER_SAMPLES = 2048;

// RAII wrapper around an audio Client object.
// Mirrors the &Client object from ssz_script/lib/sound.ssz.
class AudioClient {
public:
    // Eagerly creates the audio client via NewClient().
    // Unlike FileHandle/SocketHandle (which start closed), AudioClient
    // prepares the audio backend immediately to match SSZ's let c = Client().
    // Returns nullptr if the native plugin fails; check is_valid().
    AudioClient();
    ~AudioClient();

    // Returns true if the underlying Client was created successfully.
    bool is_valid() const { return client_ != nullptr; }

    bool start();
    bool stop();
    bool buffer_ready();
    bool set_buffer(const float* buffer, intptr_t frames);

    // Non-copyable, movable.
    AudioClient(const AudioClient&) = delete;
    AudioClient& operator=(const AudioClient&) = delete;
    AudioClient(AudioClient&& other) noexcept : client_(other.client_) { other.client_ = nullptr; }
    AudioClient& operator=(AudioClient&& other) noexcept {
        if (this != &other) { destroy(); client_ = other.client_; other.client_ = nullptr; }
        return *this;
    }

private:
    void destroy();
    void* client_ = nullptr;  // opaque Client* (defined in main/sound/sound.cpp)
};

} // namespace ikemen::ssz_native::sound
