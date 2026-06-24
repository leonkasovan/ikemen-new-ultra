
// SDL_OpenAudioDevice is used for raw float32 PCM streaming (one independent
// device per Client). SDL_mixer / SDL_OpenAudio are NOT used here to avoid
// conflicts with the BGM and SFX devices opened in sdlplugin.cpp.
#include <SDL.h>
#include "sszdef.h"

#include "typeid.h"
#include "arrayandref.hpp"
#include "pluginutil.hpp"

static const int g_srcFreq      = 48000;
static const int g_bufferSamples = 2048;
static float g_zeroBuf[g_bufferSamples * 2] = { 0 };

// ---------------------------------------------------------------------------
// Buffer – double-buffered float32 stereo ring shared between the script and
// the audio callback. Unchanged from the PortAudio version.
// ---------------------------------------------------------------------------
class Buffer
{
	float  workBuf[2][g_bufferSamples * 2];
	float* curBuf;
	float* nextBuf;
	double curOffset;
	int    nextWorkBuf;

	MEMBER void updateCurBuf()
	{
		curBuf  = nextBuf;
		nextBuf = g_zeroBuf;
	}
public:
	MEMBER Buffer() : nextBuf(g_zeroBuf), nextWorkBuf(0)
	{
		curOffset = 0;
		updateCurBuf();
	}

	// Called from the audio callback – drains `size` frames into `buffer`.
	MEMBER void writeTo(float* buffer, int size)
	{
		for (int i = 0; i < size; i++) {
			*buffer++ = curBuf[(int)curOffset * 2];
			*buffer++ = curBuf[(int)curOffset * 2 + 1];
			curOffset++;
			if (curOffset >= g_bufferSamples) {
				curOffset = 0;
				updateCurBuf();
			}
		}
	}

	// True when the next-buffer slot is free for a new write.
	MEMBER bool ready() const
	{
		return nextBuf == g_zeroBuf;
	}

	// Called from the script thread – copies one full frame of PCM.
	MEMBER bool setNextBuffer(const float* buffer)
	{
		if (!ready()) return false;
		memcpy(workBuf[nextWorkBuf], buffer, sizeof(workBuf[nextWorkBuf]));
		nextBuf      = workBuf[nextWorkBuf];
		nextWorkBuf  = !nextWorkBuf;
		return true;
	}
};

// ---------------------------------------------------------------------------
// SDL audio callback – replaces PortAudio's streamCallback.
// `len` is in bytes; divide by (float × 2 channels) to get frame count.
// ---------------------------------------------------------------------------
static void streamCallback(void* userData, Uint8* stream, int len)
{
	auto  buff   = static_cast<Buffer*>(userData);
	int   frames = len / static_cast<int>(sizeof(float) * 2);
	buff->writeTo(reinterpret_cast<float*>(stream), frames);
}

// ---------------------------------------------------------------------------
// Client – owns one SDL audio device (independent of BGM/SFX devices).
// ---------------------------------------------------------------------------
class Client
{
	SDL_AudioDeviceID device;
public:
	Buffer src;

	MEMBER Client() : device(0)
	{
		SDL_AudioSpec want, got;
		SDL_zero(want);
		SDL_zero(got);
		want.freq     = g_srcFreq;
		want.format   = AUDIO_F32SYS;
		want.channels = 2;
		want.samples  = static_cast<Uint16>(g_bufferSamples);
		want.callback = streamCallback;
		want.userdata = &src;

		device = SDL_OpenAudioDevice(nullptr, 0, &want, &got, 0);
		if (device == 0) {
			LOG_DEBUG("SDL", "Client: SDL_OpenAudioDevice failed: %s", SDL_GetError());
		} else {
			LOG_DEBUG("SDL", "Client: opened device %u (freq=%d, format=0x%04X, ch=%d)",
				device, got.freq, got.format, got.channels);
		}
	}

	MEMBER ~Client()
	{
		if (device) {
			SDL_PauseAudioDevice(device, 1);
			SDL_CloseAudioDevice(device);
			LOG_DEBUG("SDL", "Client: device %u closed", device);
		}
	}

	MEMBER SDL_AudioDeviceID dev() const { return device; }
};

// ---------------------------------------------------------------------------
// SSZ-exposed functions – identical surface as before.
// ---------------------------------------------------------------------------

extern "C" Client* SSZ_STDCALL NewClient(PluginUtil* pu)
{
	return new Client;
}

extern "C" void SSZ_STDCALL DeleteClient(PluginUtil* pu, Client* client)
{
	delete client;
}

extern "C" bool SSZ_STDCALL ClientStart(PluginUtil* pu, Client* client)
{
	if (!client->dev()) return false;
	SDL_PauseAudioDevice(client->dev(), 0);   // 0 = un-pause (start)
	LOG_DEBUG("SDL", "ClientStart: device %u started", client->dev());
	return true;
}

extern "C" bool SSZ_STDCALL ClientStop(PluginUtil* pu, Client* client)
{
	if (!client->dev()) return false;
	SDL_PauseAudioDevice(client->dev(), 1);   // 1 = pause (stop)
	LOG_DEBUG("SDL", "ClientStop: device %u stopped", client->dev());
	return true;
}

extern "C" bool SSZ_STDCALL ClientBufferReady(PluginUtil* pu, Client* client)
{
	return client->src.ready();
}

extern "C" bool SSZ_STDCALL ClientSetBuffer(PluginUtil* pu, Reference src, Client* client)
{
	if (src.len() != sizeof(float) * g_bufferSamples * 2) return false;
	return client->src.setNextBuffer(reinterpret_cast<const float*>(src.atpos()));
}
