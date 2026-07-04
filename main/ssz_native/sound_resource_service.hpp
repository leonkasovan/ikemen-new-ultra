// sound_resource_service.hpp — Native C++ scaffolding for ssz_script/ssz/sound.ssz
//
// sound.ssz (408 lines) manages game sound effects and BGM resources:
//   &SndNnm — sound group/number identifier
//   &Wave — raw PCM wave data with metadata
//   &Snd — sound effect container (loads .snd files, manages sound table)
//   &Sound — per-channel playback state with mixing
//   &Bgm — background music state
//
// Phase 5: Full implementation matching the SSZ sound.ssz behavior.
// Includes ElecbyteSnd file format parsing, WAV chunk extraction,
// sound mixing (4 format variants for mono/stereo x 8/16-bit),
// channel management, BGM play/stop via SDL_mixer delegation, and
// buffer management for the SDL audio callback.

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

namespace ikemen::ssz_native {

// ── Sound identifier ──
struct SndNnm {
	int group{-1};
	int number{};
};

// ── Wave data ──
struct WaveData {
	unsigned samplesPerSec{44100};
	unsigned short channels{0x1};
	unsigned short bytesPerSample{0x1};
	std::vector<unsigned char> wav;
	SndNnm num;

	bool is_valid() const {
		return !wav.empty() && channels >= 1 && channels <= 2
			&& (bytesPerSample == 1 || bytesPerSample == 2);
	}
};

// ── Sound container ──
// Manages a table of loaded waves keyed by (group << 32) | number.
// Provides loadFile() to parse the ElecbyteSnd format.
struct SoundTable {
	// Maps uint64_t key = (group << 32) | (uint32_t)number → WaveData
	std::unordered_map<uint64_t, WaveData> sound_map;
	uint16_t ver{};
	uint16_t ver2{};

	// Clear all loaded sounds.
	void clear() { sound_map.clear(); ver = 0; ver2 = 0; }

	// Load a .snd file (ElecbyteSnd format) and parse all embedded WAV sounds.
	// Returns empty string on success, error message on failure.
	std::string loadFile(const std::string& filename);

	// Get a sound by group+number. Returns nullptr if not found.
	const WaveData* getSound(int group, int number) const {
		uint64_t key = (static_cast<uint64_t>(static_cast<uint32_t>(group)) << 32)
			| static_cast<uint32_t>(static_cast<int32_t>(number));
		auto it = sound_map.find(key);
		return it != sound_map.end() ? &it->second : nullptr;
	}
};

// ── Per-channel sound playback state ──
struct SoundChannel {
	const WaveData* wave{nullptr};     // Active wave (null = channel free)
	int volume{256};                    // 0-512
	float x{0.0f};                      // Pan position (-160 to 160)
	bool loop_{false};
	bool lowpriority{false};
	float freqmul{1.0f};
	float fidx{0.0f};                   // Sample playback position

	void setVol(int v) {
		if (v < 0) volume = 0;
		else if (v > 512) volume = 512;
		else volume = v;
	}

	void setPan(float p) {
		if (p < -160.0f) x = -160.0f;
		else if (p > 160.0f) x = 160.0f;
		else x = p;
	}

	void setDefaultParameter() {
		setVol(256);
		loop_ = false;
		lowpriority = false;
		setPan(0.0f);
		freqmul = 1.0f;
		fidx = 0.0f;
	}

	// Mix this channel into a stereo int16 buffer.
	// left/right are pan strength multipliers (typically ±160.0).
	void mix(int* buf, float left, float right);
};

// ── BGM state ──
struct BgmData {
	std::string fileName;
	int volume{100};
	bool loop{true};

	void play(const std::string& file);
	void write() {}  // BGM playback handled by SDL_mixer — nothing to pump.
	void clear();
};

// ── Free functions ──

// Module initialization — sets up default state, config volume.
void sound_resource_init();

// ── Module-level state access ──
// Global state matching the SSZ module-level globals.

struct SoundResourceState {
	int sndbuf[2048]{};       // Stereo mix buffer (matches SNDBUFLEN from SDL plugin)
	SoundChannel sounds[16];  // 16 sound channels
	BgmData bgm;
	float panstr{128.0f};     // Pan strength from config
};

// Accessor for the internal static SoundResourceState (for testing).
SoundResourceState& sound_resource_get_state();

// Channel management.
void sndbuf_clear();
SoundChannel* get_channel(int ch);
void mix_sounds();
bool add_wave(const WaveData* wav);
void play_sound();
void stop_sound();

// Snd / sound table management (module-level).
SoundTable& get_sound_table();
std::string sound_table_load_file(const std::string& filename);
const WaveData* sound_table_get_sound(int group, int number);

} // namespace ikemen::ssz_native
