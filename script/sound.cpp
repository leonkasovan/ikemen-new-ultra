#include "sound.hpp"

#include "../main/ssz/sszdef.h"
#include "../main/ssz/typeid.h"
#include "../main/ssz/arrayandref.hpp"
#include "../main/ssz/pluginutil.hpp"

extern "C" {
	void*   SSZ_STDCALL NewClient(PluginUtil*);
	void    SSZ_STDCALL DeleteClient(PluginUtil*, void*);
	bool    SSZ_STDCALL ClientStart(PluginUtil*, void*);
	bool    SSZ_STDCALL ClientStop(PluginUtil*, void*);
	bool    SSZ_STDCALL ClientBufferReady(PluginUtil*, void*);
	bool    SSZ_STDCALL ClientSetBuffer(PluginUtil*, Reference, void*);
}

namespace ikemen {

static PluginUtil makePU()
{
	static PluginSSZFuncs f = {nullptr, nullptr, nullptr};
	static PluginUtil pu(&f, nullptr);
	return pu;
}

SoundClient::SoundClient()
{
	auto pu = makePU();
	m_cl = reinterpret_cast<intptr_t>(NewClient(&pu));
}

SoundClient::~SoundClient()
{
	if (m_cl) {
		auto pu = makePU();
		DeleteClient(&pu, reinterpret_cast<void*>(m_cl));
	}
}

bool SoundClient::start()
{
	auto pu = makePU();
	return ClientStart(&pu, reinterpret_cast<void*>(m_cl));
}

bool SoundClient::stop()
{
	auto pu = makePU();
	return ClientStop(&pu, reinterpret_cast<void*>(m_cl));
}

bool SoundClient::bufferReady()
{
	auto pu = makePU();
	return ClientBufferReady(&pu, reinterpret_cast<void*>(m_cl));
}

bool SoundClient::setBuffer(const std::vector<float>& buffer)
{
	Reference ref; ref.init();
	ref.refnew(static_cast<intptr_t>(buffer.size()), static_cast<intptr_t>(sizeof(float)));
	if (!ref.null())
		memcpy(ref.atpos(), buffer.data(), buffer.size() * sizeof(float));

	auto pu = makePU();
	bool r = ClientSetBuffer(&pu, ref, reinterpret_cast<void*>(m_cl));
	ref.releaseanddelete();
	return r;
}

} // namespace ikemen
