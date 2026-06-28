#include <windows.h>
#include <locale.h>
#include <process.h>
#include <stdint.h>

#include "vorbis/vorbisfile.h"

#include "sszdef.h"
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
	MEMBER bool open(const std::wstring& file)
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

OggVorbis* SSZ_STDCALL NewOggVorbis()
{
	return new OggVorbis;
}

void SSZ_STDCALL DeleteOggVorbis(OggVorbis* ov)
{
	delete ov;
}

bool SSZ_STDCALL OggVorbisOpen(const std::wstring& file, OggVorbis* ov)
{
	std::string fname;
	{
		int len = WideCharToMultiByte(CP_THREAD_ACP, 0, file.c_str(), (int)file.size(), nullptr, 0, nullptr, nullptr);
		if (len > 0) {
			fname.resize(len);
			WideCharToMultiByte(CP_THREAD_ACP, 0, file.c_str(), (int)file.size(), &fname[0], len, nullptr, nullptr);
		}
	}
	MEM_MARK_BEFORE_NAMED(MUSIC, fname.c_str());
	bool result = ov->open(file);
	MEM_MARK_AFTER_NAMED(MUSIC, fname.c_str());
	return result;
}

void SSZ_STDCALL OggVorbisClear(OggVorbis* ov)
{
	ov->clear();
}

int64_t SSZ_STDCALL OggVorbisPcmTotal(OggVorbis* ov)
{
	return ov->pcmTotal();
}

int32_t SSZ_STDCALL OggVorbisChannels(OggVorbis* ov)
{
	return ov->channels();
}

int32_t SSZ_STDCALL OggVorbisRate(OggVorbis* ov)
{
	return ov->rate();
}

intptr_t SSZ_STDCALL OggVorbisRead(int16_t* buffer, intptr_t length, OggVorbis* ov)
{
	if(length == 0) return 0;
	return ov->read(buffer, length);
}

int32_t SSZ_STDCALL OggVorbisSeek(double time, OggVorbis* ov)
{
	return ov->seek(time);
}
