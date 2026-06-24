#pragma once

#include "../table.hpp"
#include "../alpha/sdlplugin.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace ikemen {

// ── Forward declarations ─────────────────────────────────────────────────

struct SndNnm { int group = -1, number = 0; };

struct Wave {
	uint32_t  samplesPerSec = 44100;
	uint16_t  channels = 1, bytesPerSample = 1;
	std::vector<uint8_t> wav;
	SndNnm    num;
};

// ── Sound file loader (.snd / ElecbyteSnd) ──────────────────────────────

class Snd {
public:
	void               clear();
	std::wstring       loadFile(const std::wstring& filename);
	Wave*              getSound(int group, int number);

private:
	ikemen::IntTable<uint64_t, Wave> m_soundTable;
	uint16_t m_ver = 0, m_ver2 = 0;
};

// ── PCM mixer ────────────────────────────────────────────────────────────

class Sound {
public:
	Wave*      sound = nullptr;
	float      x = 0.0f;
	int16_t    volume = 256;
	bool       loop = false, lowpriority = false;
	float      freqmul = 1.0f;
	float      fidx = 0.0f;

	static constexpr float FRAME_TIME = 1.0f / static_cast<float>(SNDFREQ);

	void setVol(int v);
	void setPan(float p);
	void setDefaultParameter();
	void mix(std::vector<int32_t>& buf, float left, float right);

private:
	void mix_s16(std::vector<int32_t>& buf, float fidxadd, int lv, int rv);
	void mix_m16(std::vector<int32_t>& buf, float fidxadd, int lv, int rv);
	void mix_s8(std::vector<int32_t>& buf, float fidxadd, int lv, int rv);
	void mix_m8(std::vector<int32_t>& buf, float fidxadd, int lv, int rv);
};

// ── BGM playback ─────────────────────────────────────────────────────────

class Bgm {
public:
	void play(const std::wstring& file);
	void clear();

private:
	std::wstring m_fileName;
};

// ── Global sound state ───────────────────────────────────────────────────

extern float                       g_panstr;
extern std::vector<Sound>       g_sounds;
extern std::vector<int32_t>     g_sndbuf;
extern Bgm                        g_bgm;

void sndbufClear();
Sound* getChannel(int ch);
void  mixSounds();
bool  addWave(Wave* wav);
void  playSound();
void  stopSound();

} // namespace ikemen
