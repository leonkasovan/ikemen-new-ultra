#include "socket.hpp"

#include "../main/ssz/sszdef.h"
#include "../main/ssz/typeid.h"
#include "../main/ssz/arrayandref.hpp"
#include "../main/ssz/pluginutil.hpp"

extern "C" {
	void     SSZ_STDCALL SocketClose(PluginUtil*, intptr_t*);
	bool     SSZ_STDCALL SocketConnect(PluginUtil*, bool, int32_t, Reference, Reference, intptr_t*);
	bool     SSZ_STDCALL SocketListen(PluginUtil*, bool, int32_t, Reference, intptr_t*);
	intptr_t SSZ_STDCALL SocketAccept(PluginUtil*, bool, int32_t, intptr_t);
	bool     SSZ_STDCALL SocketSend(PluginUtil*, intptr_t, void*, intptr_t*);
	intptr_t SSZ_STDCALL SocketSendAry(PluginUtil*, intptr_t, Reference, intptr_t*);
	bool     SSZ_STDCALL SocketRecv(PluginUtil*, intptr_t, void*, intptr_t*);
	intptr_t SSZ_STDCALL SocketRecvAry(PluginUtil*, intptr_t, Reference, intptr_t*);
}

namespace ikemen {

static PluginUtil makePU()
{
	static PluginSSZFuncs f = {nullptr, nullptr, nullptr};
	static PluginUtil pu(&f, nullptr);
	return pu;
}

Socket::~Socket() { close(); }

bool Socket::isOpen() const { return m_soc != INVALID_SOCKET; }

void Socket::close()
{
	if (m_soc != INVALID_SOCKET) {
		auto pu = makePU();
		SocketClose(&pu, &m_soc);
		m_soc = INVALID_SOCKET;
	}
}

bool Socket::connect(const std::wstring& host, const std::wstring& port, int timeout, bool nodelay)
{
	close();
	Reference rHost; rHost.init();
	Reference rPort; rPort.init();
	PluginUtil::wstrToRef(rHost, host);
	PluginUtil::wstrToRef(rPort, port);
	auto pu = makePU();
	bool ok = SocketConnect(&pu, nodelay, static_cast<int32_t>(timeout), rPort, rHost, &m_soc);
	if (!ok) m_soc = INVALID_SOCKET;
	rHost.releaseanddelete();
	rPort.releaseanddelete();
	return ok;
}

bool Socket::listen(const std::wstring& port, int backlog, bool ipv4)
{
	close();
	Reference rPort; rPort.init();
	PluginUtil::wstrToRef(rPort, port);
	auto pu = makePU();
	bool ok = SocketListen(&pu, ipv4, static_cast<int32_t>(backlog), rPort, &m_soc);
	if (!ok) m_soc = INVALID_SOCKET;
	rPort.releaseanddelete();
	return ok;
}

bool Socket::accept(Socket& out, int timeout, bool nodelay)
{
	out.close();
	auto pu = makePU();
	intptr_t s = SocketAccept(&pu, nodelay, static_cast<int32_t>(timeout), m_soc);
	out.m_soc = s;
	return s != INVALID_SOCKET;
}

namespace detail {

void socketSendImpl(intptr_t* soc, intptr_t size, const void* data)
{
	auto pu = makePU();
	SocketSend(&pu, size, const_cast<void*>(data), soc);
}

intptr_t socketSendAryImpl(intptr_t* soc, intptr_t elemSize, intptr_t count, const void* data)
{
	Reference r; r.init();
	r.refnew(count * elemSize, 1);
	if (!r.null()) memcpy(r.atpos(), data, static_cast<size_t>(count * elemSize));
	auto pu = makePU();
	intptr_t n = SocketSendAry(&pu, elemSize, r, soc);
	r.releaseanddelete();
	return n;
}

bool socketRecvImpl(intptr_t* soc, intptr_t size, void* out)
{
	auto pu = makePU();
	return SocketRecv(&pu, size, out, soc);
}

intptr_t socketRecvAryImpl(intptr_t* soc, intptr_t elemSize, intptr_t count, void* out)
{
	Reference r; r.init();
	r.refnew(count * elemSize, 1);
	auto pu = makePU();
	intptr_t n = SocketRecvAry(&pu, elemSize, r, soc);
	if (n > 0 && n <= count * elemSize)
		memcpy(out, r.atpos(), static_cast<size_t>(n));
	r.releaseanddelete();
	return n;
}

} // namespace detail

} // namespace ikemen
