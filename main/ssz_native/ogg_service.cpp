#include "ogg_service.hpp"

#ifndef SSZ_STDCALL
#define SSZ_STDCALL __stdcall
#endif

// Forward declaration matching main/ogg/ogg.cpp
class OggVorbis;

// Native ogg plugin functions (defined in main/ogg/ogg.cpp).
// These declarations duplicate bridge.cpp:121-129. They are tracked in
// plugin_native_api.hpp's M4 TODO for eventual consolidation.
// TODO: Move to plugin_native_api.hpp when ogg is migrated (Phase 2).
OggVorbis* SSZ_STDCALL NewOggVorbis();
void       SSZ_STDCALL DeleteOggVorbis(OggVorbis* ov);
bool       SSZ_STDCALL OggVorbisOpen(const std::wstring& file, OggVorbis* ov);
void       SSZ_STDCALL OggVorbisClear(OggVorbis* ov);
int64_t    SSZ_STDCALL OggVorbisPcmTotal(OggVorbis* ov);
int32_t    SSZ_STDCALL OggVorbisChannels(OggVorbis* ov);
int32_t    SSZ_STDCALL OggVorbisRate(OggVorbis* ov);
intptr_t   SSZ_STDCALL OggVorbisRead(int16_t* buffer, intptr_t length, OggVorbis* ov);
int32_t    SSZ_STDCALL OggVorbisSeek(double time, OggVorbis* ov);

namespace ikemen::ssz_native::ogg {

OggVorbisHandle::OggVorbisHandle() {
    ptr_ = static_cast<void*>(NewOggVorbis());
}

OggVorbisHandle::~OggVorbisHandle() {
    destroy();
}

void OggVorbisHandle::destroy() {
    if (ptr_) {
        DeleteOggVorbis(static_cast<OggVorbis*>(ptr_));
        ptr_ = nullptr;
    }
}

bool OggVorbisHandle::open(const std::wstring& file) {
    return ptr_ ? OggVorbisOpen(file, static_cast<OggVorbis*>(ptr_)) : false;
}

void OggVorbisHandle::clear() {
    if (ptr_) OggVorbisClear(static_cast<OggVorbis*>(ptr_));
}

int64_t OggVorbisHandle::pcm_total() {
    return ptr_ ? OggVorbisPcmTotal(static_cast<OggVorbis*>(ptr_)) : 0;
}

int32_t OggVorbisHandle::channels() {
    return ptr_ ? OggVorbisChannels(static_cast<OggVorbis*>(ptr_)) : 0;
}

int32_t OggVorbisHandle::rate() {
    return ptr_ ? OggVorbisRate(static_cast<OggVorbis*>(ptr_)) : 0;
}

intptr_t OggVorbisHandle::read(int16_t* buffer, intptr_t length) {
    return ptr_ ? OggVorbisRead(buffer, length, static_cast<OggVorbis*>(ptr_)) : -1;
}

int32_t OggVorbisHandle::seek(double time) {
    return ptr_ ? OggVorbisSeek(time, static_cast<OggVorbis*>(ptr_)) : -1;
}

} // namespace ikemen::ssz_native::ogg
