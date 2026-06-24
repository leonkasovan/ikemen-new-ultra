#include "sound.hpp"
#include "common.hpp"

#include "../math.hpp"
#include "../string.hpp"
#include "../file.hpp"
#include "../save/config.hpp"
#include "../alpha/sdlplugin.hpp"

#include <algorithm>
#include <cstring>

namespace ikemen {

// ── Snd ──────────────────────────────────────────────────────────────────

void Snd::clear() { m_soundTable.clear(); }

std::wstring Snd::loadFile(const std::wstring& filename)
{
	File f;
	if (!f.open(filename, L"rb")) return L"Failed to open file";

	clear();

	uint32_t subHeaderOffset = 0, subFileLength = 0, numberOfSounds = 0;

	auto wavLoader = [&](Wave& wav) -> std::wstring {
		uint32_t riffSize = 0, fmtSize = 0, dataSize = 0, musi;
		uint8_t ub;

		// Read "RIFF"
		{
			std::wstring s;
			for (int i = 0; i < 4; i++) { if (!f.read(ub)) return L"File could not be loaded"; s += static_cast<wchar_t>(ub); }
			if (s != L"RIFF") return L"Not RIFF";
		}

		if (!f.read(riffSize)) return L"File could not be loaded";
		riffSize += 8;

		// Read "WAVE"
		{
			std::wstring s;
			for (int i = 0; i < 4; i++) { if (!f.read(ub)) return L"File could not be loaded"; s += static_cast<wchar_t>(ub); }
			if (s != L"WAVE") return L"";
		}

		while (true) {
			uint16_t fmtID, mushi;
			std::wstring s;
			int64_t ofs = static_cast<int64_t>(subHeaderOffset) + 28;
			uint32_t size;

			// Read chunk ID
			s.clear();
			for (int i = 0; i < 4; i++) {
				if (!f.read(ub)) goto end_loop;
				s += static_cast<wchar_t>(ub);
			}
			if (!f.read(size)) break;

			if (s == L"fmt ") {
				fmtSize = size;
				if (!f.read(fmtID)) return L"File could not be loaded";
				if (fmtID != 1) return L"not linear PCM";
				if (!f.read(wav.channels)) return L"File could not be loaded";
				if (wav.channels - 1 >= 2) return L"Incorrect number of channels";
				if (!f.read(wav.samplesPerSec)) return L"File could not be loaded";
				if (wav.samplesPerSec - 1 >= 0x100000)
					return L"Incorrect frequency " + std::to_wstring(static_cast<int>(wav.samplesPerSec));
				if (!f.read(musi)) return L"File could not be loaded";
				if (!f.read(mushi)) return L"File could not be loaded";
				if (!f.read(wav.bytesPerSample)) return L"File could not be loaded";
				if (wav.bytesPerSample != 8 && wav.bytesPerSample != 16)
					return L"invalid number of bits";
				wav.bytesPerSample >>= 3;
			}
			else if (s == L"data") {
				dataSize = size;
				wav.wav.resize(static_cast<size_t>(dataSize));
				f.readAry(wav.wav);
			}

			ofs += static_cast<int64_t>(size) + 8;
			f.seek(ofs, Seek::SET);

			if (!(fmtSize == 0 || dataSize == 0)) break;
			if (ofs >= static_cast<int64_t>(subHeaderOffset) + static_cast<int64_t>(riffSize) + 16) break;
		}
		end_loop:

		if (fmtSize == 0 && dataSize > 0) return L"no fmt";
		return L"";
	};

	// Read header "ElecbyteSnd"
	{
		uint8_t ub;
		std::wstring s;
		for (int i = 0; i < 12; i++) { if (!f.read(ub)) return L"File could not be loaded"; s += static_cast<wchar_t>(ub); }
		if (s.compare(0, 11, L"ElecbyteSnd") != 0) return L"Not ElecbyteSnd";
	}

	if (!f.read(m_ver))  return L"File could not be loaded";
	if (!f.read(m_ver2)) return L"File could not be loaded";
	if (!f.read(numberOfSounds))     return L"File could not be loaded";
	if (!f.read(subHeaderOffset))    return L"File could not be loaded";

	for (uint32_t i = 0; i < numberOfSounds; i++) {
		uint32_t nextSubHeaderOffset;
		std::vector<Wave> soundList(static_cast<size_t>(numberOfSounds));

		f.seek(static_cast<int64_t>(subHeaderOffset), Seek::SET);
		if (!f.read(nextSubHeaderOffset)) return L"File could not be loaded";
		if (!f.read(subFileLength))       return L"File could not be loaded";
		if (!f.read(soundList[i].num.group))  return L"File could not be loaded";
		if (!f.read(soundList[i].num.number)) return L"File could not be loaded";

		if (soundList[i].num.group >= 0 && soundList[i].num.number >= 0) {
			uint64_t key = (static_cast<uint64_t>(soundList[i].num.group) << 32)
			             | static_cast<uint32_t>(soundList[i].num.number);

			auto* existing = m_soundTable.getPtr(key);
			if (existing && !existing->wav.empty()) continue;

			std::wstring error = wavLoader(soundList[i]);
			if (!error.empty()) return error;
			m_soundTable.set(key, soundList[i]);
		}
		subHeaderOffset = nextSubHeaderOffset;
	}
	return L"";
}

Wave* Snd::getSound(int group, int number)
{
	uint64_t key = (static_cast<uint64_t>(group) << 32) | static_cast<uint32_t>(number);
	return m_soundTable.getPtr(key);
}

// ── Sound (mixer) ────────────────────────────────────────────────────────

void Sound::setVol(int v)
{
	if (v < 0) volume = 0;
	else if (v > 512) volume = 512;
	else volume = static_cast<int16_t>(v);
}

void Sound::setPan(float p)
{
	if (sound && !sound->wav.empty()) {
		x = p;
	}
	if (p < -160.0f) x = -160.0f;
	else if (p > 160.0f) x = 160.0f;
	else x = p;
}

void Sound::setDefaultParameter()
{
	setVol(256);
	loop = false;
	lowpriority = false;
	setPan(0.0f);
	freqmul = 1.0f;
	fidx = 0.0f;
}

void Sound::mix_s16(std::vector<int32_t>& buf, float fidxadd, int lv, int rv)
{
	auto& w = sound->wav;
	size_t l = w.size();

	for (int i = 0; i < SNDBUFLEN; ) {
		int iidx = static_cast<int>(fidx) * 4;
		if (static_cast<size_t>(iidx) >= l) {
			if (!loop) { sound = nullptr; return; }
			iidx = 0; fidx = 0.0f;
		}
		buf[i]   += (static_cast<int>(w[iidx]) | (static_cast<int>(static_cast<int8_t>(w[iidx+1]))<<8)) * lv >> 8;
		buf[i+1] += (static_cast<int>(w[iidx+2]) | (static_cast<int>(static_cast<int8_t>(w[iidx+3]))<<8)) * rv >> 8;
		i += 2;
		fidx += fidxadd;
	}
}

void Sound::mix_m16(std::vector<int32_t>& buf, float fidxadd, int lv, int rv)
{
	auto& w = sound->wav;
	size_t l = w.size();

	for (int i = 0; i < SNDBUFLEN; ) {
		int iidx = static_cast<int>(fidx) * 2;
		if (static_cast<size_t>(iidx) >= l) {
			if (!loop) { sound = nullptr; return; }
			iidx = 0; fidx = 0.0f;
		}
		int tmp = (static_cast<int>(w[iidx]) | (static_cast<int>(static_cast<int8_t>(w[iidx+1]))<<8));
		buf[i]   += tmp * lv >> 8;
		buf[i+1] += tmp * rv >> 8;
		i += 2;
		fidx += fidxadd;
	}
}

void Sound::mix_s8(std::vector<int32_t>& buf, float fidxadd, int lv, int rv)
{
	auto& w = sound->wav;
	size_t l = w.size();

	for (int i = 0; i < SNDBUFLEN; ) {
		int iidx = static_cast<int>(fidx) * 2;
		if (static_cast<size_t>(iidx) >= l) {
			if (!loop) { sound = nullptr; return; }
			iidx = 0; fidx = 0.0f;
		}
		buf[i]   += (static_cast<int>(w[iidx]) - 128) * lv;
		buf[i+1] += (static_cast<int>(w[iidx+1]) - 128) * rv;
		i += 2;
		fidx += fidxadd;
	}
}

void Sound::mix_m8(std::vector<int32_t>& buf, float fidxadd, int lv, int rv)
{
	auto& w = sound->wav;
	size_t l = w.size();

	for (int i = 0; i < SNDBUFLEN; ) {
		int iidx = static_cast<int>(fidx);
		if (static_cast<size_t>(iidx) >= l) {
			if (!loop) { sound = nullptr; return; }
			iidx = 0; fidx = 0.0f;
		}
		int tmp = static_cast<int>(w[iidx]) - 128;
		buf[i]   += tmp * lv;
		buf[i+1] += tmp * rv;
		i += 2;
		fidx += fidxadd;
	}
}

void Sound::mix(std::vector<int32_t>& buf, float left, float right)
{
	if (!sound || sound->wav.empty()) return;
	float fidxadd = static_cast<float>(sound->samplesPerSec) * freqmul * FRAME_TIME;
	if (!ikemen::isfinite(fidxadd) || fidxadd <= 0.0f) { sound = nullptr; return; }

	int lv = volume, rv = volume;
	if (sound->channels == 2) {
		if (sound->bytesPerSample == 2) mix_s16(buf, fidxadd, lv, rv);
		else                             mix_s8(buf, fidxadd, lv, rv);
	} else {
		if (sound->bytesPerSample == 2)
			mix_m16(buf, fidxadd, static_cast<int>(static_cast<float>(lv)-(x*g_panstr)),
			                    static_cast<int>(static_cast<float>(rv)+(x*g_panstr)));
		else
			mix_m8(buf, fidxadd, static_cast<int>(static_cast<float>(lv)-(x*g_panstr)),
			                   static_cast<int>(static_cast<float>(rv)+(x*g_panstr)));
	}
}

// ── Bgm ──────────────────────────────────────────────────────────────────

void Bgm::play(const std::wstring& file) { clear(); if (ikemen::playBGM(L"", file)) m_fileName = file; }
void Bgm::clear() { m_fileName.clear(); ikemen::playBGM(L"", L""); }

// ── Global state ─────────────────────────────────────────────────────────

float g_panstr = ikemen::config::PanStr;
std::vector<Sound>   g_sounds(16);
std::vector<int32_t> g_sndbuf(ikemen::SNDBUFLEN);
Bgm g_bgm;

void sndbufClear()
{
	std::fill(g_sndbuf.begin(), g_sndbuf.end(), 0);
}

Sound* getChannel(int ch)
{
	int c = ikemen::min(15, ch);
	if (c >= 0) return &g_sounds[static_cast<size_t>(c)];
	for (int i = 15; i >= 0; i--) {
		if (!g_sounds[static_cast<size_t>(i)].sound || g_sounds[static_cast<size_t>(i)].sound->wav.empty()) {
			g_sounds[static_cast<size_t>(i)].setDefaultParameter();
			return &g_sounds[static_cast<size_t>(i)];
		}
	}
	return nullptr;
}

void mixSounds()
{
	g_bgm.clear(); // actually write was empty
	for (auto& s : g_sounds) s.mix(g_sndbuf, -160.0f, 160.0f);
}

bool addWave(Wave* wav)
{
	if (!wav || wav->wav.empty()) return false;
	Sound* c = getChannel(-1);
	if (!c) return false;
	c->sound = wav;
	return true;
}

void playSound()
{
	if (ikemen::setSndBuf(g_sndbuf)) {
		sndbufClear();
		mixSounds();
	}
}

void stopSound()
{
	sndbufClear();
	g_sounds.clear();
	g_sounds.resize(16);
}

} // namespace ikemen
