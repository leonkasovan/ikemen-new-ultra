#include "sound_service.hpp"

#ifndef SSZ_STDCALL
#define SSZ_STDCALL __stdcall
#endif

// Forward declaration matching main/sound/sound.cpp
class Client;

// Native sound plugin functions (defined in main/sound/sound.cpp).
// These declarations duplicate bridge.cpp:79-84. They are tracked in
// plugin_native_api.hpp's M4 TODO for eventual consolidation.
// TODO: Move to plugin_native_api.hpp when sound is migrated (Phase 2).
Client*   SSZ_STDCALL NewClient();
void      SSZ_STDCALL DeleteClient(Client* client);
bool      SSZ_STDCALL ClientStart(Client* client);
bool      SSZ_STDCALL ClientStop(Client* client);
bool      SSZ_STDCALL ClientBufferReady(Client* client);
bool      SSZ_STDCALL ClientSetBuffer(const float* buffer, intptr_t frames, Client* client);

namespace ikemen::ssz_native::sound {

AudioClient::AudioClient() {
    client_ = static_cast<void*>(NewClient());
}

AudioClient::~AudioClient() {
    destroy();
}

void AudioClient::destroy() {
    if (client_) {
        DeleteClient(static_cast<Client*>(client_));
        client_ = nullptr;
    }
}

bool AudioClient::start() {
    return client_ ? ClientStart(static_cast<Client*>(client_)) : false;
}

bool AudioClient::stop() {
    return client_ ? ClientStop(static_cast<Client*>(client_)) : false;
}

bool AudioClient::buffer_ready() {
    return client_ ? ClientBufferReady(static_cast<Client*>(client_)) : false;
}

bool AudioClient::set_buffer(const float* buffer, intptr_t frames) {
    return client_ ? ClientSetBuffer(buffer, frames, static_cast<Client*>(client_)) : false;
}

} // namespace ikemen::ssz_native::sound
