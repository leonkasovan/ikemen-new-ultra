
#include <windows.h>
#include <locale.h>
#include <process.h>
#include <stdint.h>

#include "vorbis/vorbisfile.h"

#include "sszdef.h"
#include "typeid.h"
#include "arrayandref.hpp"
#include "pluginutil.hpp"
#include "mem_profiler.hpp"


class OggVorbis
{
	OggVorbis_File _vf;
	FILE* _fh;
	MEMBER void fileClose()
	{
		if (_fh) fclose(_fh);
		_fh = nullptr;
	}
public:
	MEMBER OggVorbis()
	{
		memset(&_vf, 0, sizeof(_vf));
		_fh = nullptr;
	}
	MEMBER ~OggVorbis()
	{
		clear();
	}
	MEMBER bool open(std::WSTR file)
	{
		clear();
		_wfopen_s(&_fh, file.c_str(), L("rb"));
		if (!_fh) return false;
		if (ov_open(_fh, &_vf, nullptr, 0) < 0) {
			fileClose();
			return false;
		}
		return true;
	}
	MEMBER void clear()
	{
		if (!_fh) return;
		ov_clear(&_vf);
		_fh = nullptr;
	}
	MEMBER int64_t pcmTotal()
	{
		return ov_pcm_total(&_vf, -1);
	}
	MEMBER int32_t channels()
	{
		auto nc = ov_info(&_vf, -1);
		return nc ? nc->channels : -1;
	}
	MEMBER int32_t rate()
	{
		auto nc = ov_info(&_vf, -1);
		return nc ? nc->rate : -1;
	}
	MEMBER intptr_t read(int16_t* buffer, intptr_t length)
	{
		int current_section;
		auto rlen =
			ov_read(&_vf, (char*)buffer, length * 2, 0, 2, 1, &current_section);
		if (rlen > 0) rlen /= 2;
		return rlen;
	}
	MEMBER int32_t seek(double time)
	{
		return ov_time_seek(&_vf, time);
	}
};

extern "C" OggVorbis* SSZ_STDCALL NewOggVorbis(PluginUtil* pu)
{
	return new OggVorbis;
}

extern "C" void SSZ_STDCALL DeleteOggVorbis(PluginUtil* pu, OggVorbis* ov)
{
	delete ov;
}

extern "C" bool SSZ_STDCALL OggVorbisOpen(PluginUtil* pu, Reference file, OggVorbis* ov)
{
	std::string fname = pu->refToAstr(CP_THREAD_ACP, file);
	MEM_MARK_BEFORE_NAMED(MUSIC, fname.c_str());
	bool result = ov->open(pu->refToWstr(file));
	MEM_MARK_AFTER_NAMED(MUSIC, fname.c_str());
	return result;
}

extern "C" void SSZ_STDCALL OggVorbisClear(PluginUtil* pu, OggVorbis* ov)
{
	ov->clear();
}

extern "C" int64_t SSZ_STDCALL OggVorbisPcmTotal(PluginUtil* pu, OggVorbis* ov)
{
	return ov->pcmTotal();
}

extern "C" int32_t SSZ_STDCALL OggVorbisChannels(PluginUtil* pu, OggVorbis* ov)
{
	return ov->channels();
}

extern "C" int32_t SSZ_STDCALL OggVorbisRate(PluginUtil* pu, OggVorbis* ov)
{
	return ov->rate();
}

extern "C" intptr_t SSZ_STDCALL OggVorbisRead(PluginUtil* pu, Reference buffer, OggVorbis* ov)
{
	if(buffer.len() == 0) return 0;
	return ov->read((int16_t*)buffer.atpos(), buffer.len()/sizeof(int16_t));
}

extern "C" int32_t SSZ_STDCALL OggVorbisSeek(PluginUtil* pu, double time, OggVorbis* ov)
{
	return ov->seek(time);
}

