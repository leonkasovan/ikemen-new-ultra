#include "socket_service.hpp"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
typedef int SOCKET;
#endif

#ifndef SSZ_STDCALL
#define SSZ_STDCALL __stdcall
#endif

// Native socket plugin functions (defined in main/socket/socket.cpp).
// These declarations duplicate bridge.cpp:70-80. They are tracked in
// plugin_native_api.hpp's M4 TODO for eventual consolidation.
// TODO: Move to plugin_native_api.hpp when socket is migrated (Phase 2).
void     SSZ_STDCALL SocketClose(SOCKET* psoc);
bool     SSZ_STDCALL SocketConnect(bool nodelay, int32_t timeout, const std::string& port, const std::string& host, SOCKET* psoc);
bool     SSZ_STDCALL SocketListen(bool ipv4, int32_t backlog, const std::string& port, SOCKET* psoc);
SOCKET   SSZ_STDCALL SocketAccept(bool nodelay, int32_t timeout, SOCKET soc);
bool     SSZ_STDCALL SocketSend(intptr_t size, const char* p, SOCKET* psoc);
intptr_t SSZ_STDCALL SocketSendAry(intptr_t size, const void* data, intptr_t bytes, SOCKET* psoc);
bool     SSZ_STDCALL SocketRecv(intptr_t size, char* p, SOCKET* psoc);
intptr_t SSZ_STDCALL SocketRecvAry(intptr_t size, void* data, intptr_t bytes, SOCKET* psoc);

namespace ikemen::ssz_native::socket {

bool SocketHandle::connect(const std::string& host, const std::string& port, int32_t timeout, bool nodelay) {
    close();
    SOCKET s = static_cast<SOCKET>(SOCKET_SENTINEL);
    bool ok = SocketConnect(nodelay, timeout, port, host, &s);
    if (ok) soc_ = static_cast<intptr_t>(s);
    return ok;
}

bool SocketHandle::listen(const std::string& port, int32_t backlog, bool ipv4) {
    close();
    SOCKET s = static_cast<SOCKET>(SOCKET_SENTINEL);
    bool ok = SocketListen(ipv4, backlog, port, &s);
    if (ok) soc_ = static_cast<intptr_t>(s);
    return ok;
}

SocketHandle SocketHandle::accept(int32_t timeout, bool nodelay) {
    SOCKET accepted = SocketAccept(nodelay, timeout, static_cast<SOCKET>(soc_));
    SocketHandle client;
    client.soc_ = static_cast<intptr_t>(accepted);
    return client;
}

void SocketHandle::close() {
    if (soc_ != SOCKET_SENTINEL) {
        SOCKET s = static_cast<SOCKET>(soc_);
        SocketClose(&s);
        soc_ = SOCKET_SENTINEL;
    }
}

bool SocketHandle::recv(intptr_t size, char* data) {
    SOCKET s = static_cast<SOCKET>(soc_);
    return SocketRecv(size, data, &s);
}

intptr_t SocketHandle::recv_array(intptr_t elem_size, void* data, intptr_t count) {
    SOCKET s = static_cast<SOCKET>(soc_);
    return SocketRecvAry(elem_size, data, count * elem_size, &s);
}

bool SocketHandle::send(intptr_t size, const char* data) {
    SOCKET s = static_cast<SOCKET>(soc_);
    return SocketSend(size, data, &s);
}

intptr_t SocketHandle::send_array(intptr_t elem_size, const void* data, intptr_t count) {
    SOCKET s = static_cast<SOCKET>(soc_);
    return SocketSendAry(elem_size, data, count * elem_size, &s);
}

} // namespace ikemen::ssz_native::socket
