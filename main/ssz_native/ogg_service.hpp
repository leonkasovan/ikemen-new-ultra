#pragma once

// ogg_service.hpp — Native equivalent of ssz_script/lib/alpha/ogg.ssz
//
// Provides:
// - RAII wrapper for OggVorbis decoder object
// - open, clear, pcmTotal, channels, rate, read, seek
//
// Design note: ogg_service delegates to the SSZ native plugin layer
// (main/ogg/ogg.cpp). Audio decoding depends on libvorbis managed by
// the native plugin. Using the call-through pattern (mirroring sound_service
// and file_service) avoids duplicating vorbis decoder initialization.

#include <cstdint>
#include <string>

namespace ikemen::ssz_native::ogg {

// RAII wrapper around an OggVorbis decoder object.
// Mirrors the &OggVorbis object from ssz_script/lib/alpha/ogg.ssz.
class OggVorbisHandle {
public:
    // Eagerly creates the OggVorbis decoder via NewOggVorbis().
    OggVorbisHandle();
    ~OggVorbisHandle();

    // Returns true if the underlying decoder was created successfully.
    bool is_valid() const { return ptr_ != nullptr; }

    // Open an Ogg Vorbis file. Returns true on success.
    bool open(const std::wstring& file);

    // Clear the current file (keeps the decoder object valid for reuse).
    void clear();

    // Total PCM samples in the current file.
    int64_t pcm_total();

    // Number of audio channels.
    int32_t channels();

    // Sample rate in Hz.
    int32_t rate();

    // Read up to `length` PCM samples into buffer.
    // Returns the number of samples actually read.
    intptr_t read(int16_t* buffer, intptr_t length);

    // Seek to time (in seconds). Returns 0 on success.
    int32_t seek(double time);

    // Non-copyable, movable.
    OggVorbisHandle(const OggVorbisHandle&) = delete;
    OggVorbisHandle& operator=(const OggVorbisHandle&) = delete;
    OggVorbisHandle(OggVorbisHandle&& other) noexcept : ptr_(other.ptr_) { other.ptr_ = nullptr; }
    OggVorbisHandle& operator=(OggVorbisHandle&& other) noexcept {
        if (this != &other) { destroy(); ptr_ = other.ptr_; other.ptr_ = nullptr; }
        return *this;
    }

private:
    void destroy();
    void* ptr_ = nullptr;  // opaque OggVorbis* (defined in main/ogg/ogg.cpp)
};

} // namespace ikemen::ssz_native::ogg
