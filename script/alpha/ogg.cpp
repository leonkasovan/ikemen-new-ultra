#include "ogg.hpp"

#include "sszdef.h"
#include "typeid.h"
#include "arrayandref.hpp"
#include "pluginutil.hpp"

extern "C" {
	void*    SSZ_STDCALL NewOggVorbis(PluginUtil*);
	void     SSZ_STDCALL DeleteOggVorbis(PluginUtil*, void*);
	bool     SSZ_STDCALL OggVorbisOpen(PluginUtil*, Reference, void*);
	void     SSZ_STDCALL OggVorbisClear(PluginUtil*, void*);
	int64_t  SSZ_STDCALL OggVorbisPcmTotal(PluginUtil*, void*);
	int32_t  SSZ_STDCALL OggVorbisChannels(PluginUtil*, void*);
	int32_t  SSZ_STDCALL OggVorbisRate(PluginUtil*, void*);
	intptr_t SSZ_STDCALL OggVorbisRead(PluginUtil*, Reference, void*);
	int32_t  SSZ_STDCALL OggVorbisSeek(PluginUtil*, double, void*);
}

namespace ikemen {

static PluginUtil makePU()
{
	static PluginSSZFuncs f = {nullptr, nullptr, nullptr};
	static PluginUtil pu(&f, nullptr);
	return pu;
}

OggVorbis::OggVorbis()
{
	auto pu = makePU();
	m_ptr = reinterpret_cast<intptr_t>(NewOggVorbis(&pu));
}

OggVorbis::~OggVorbis()
{
	if (m_ptr) {
		auto pu = makePU();
		DeleteOggVorbis(&pu, reinterpret_cast<void*>(m_ptr));
	}
}

bool OggVorbis::open(const std::wstring& file)
{
	Reference r; r.init();
	PluginUtil::wstrToRef(r, file);
	auto pu = makePU();
	bool ok = OggVorbisOpen(&pu, r, reinterpret_cast<void*>(m_ptr));
	r.releaseanddelete();
	return ok;
}

void OggVorbis::clear()
{
	auto pu = makePU();
	OggVorbisClear(&pu, reinterpret_cast<void*>(m_ptr));
}

int64_t OggVorbis::pcmTotal()
{
	auto pu = makePU();
	return OggVorbisPcmTotal(&pu, reinterpret_cast<void*>(m_ptr));
}

int32_t OggVorbis::channels()
{
	auto pu = makePU();
	return OggVorbisChannels(&pu, reinterpret_cast<void*>(m_ptr));
}

int32_t OggVorbis::rate()
{
	auto pu = makePU();
	return OggVorbisRate(&pu, reinterpret_cast<void*>(m_ptr));
}

intptr_t OggVorbis::read(std::vector<int16_t>& buffer)
{
	Reference r; r.init();
	if (!buffer.empty()) {
		r.refnew(static_cast<intptr_t>(buffer.size()), sizeof(int16_t));
	}
	auto pu = makePU();
	intptr_t n = OggVorbisRead(&pu, r, reinterpret_cast<void*>(m_ptr));
	if (n > 0 && static_cast<size_t>(n) <= buffer.size())
		memcpy(buffer.data(), r.atpos(), static_cast<size_t>(n) * sizeof(int16_t));
	r.releaseanddelete();
	return n;
}

int32_t OggVorbis::seek(double time)
{
	auto pu = makePU();
	return OggVorbisSeek(&pu, time, reinterpret_cast<void*>(m_ptr));
}

} // namespace ikemen
