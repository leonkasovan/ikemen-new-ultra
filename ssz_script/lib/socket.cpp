// ============================================================================
// socket.cpp — native (C++) implementation of the SSZ `socket` library's
// `&Socket` struct methods.
//
// Consumed from ssz_script/lib/socket.ssz, which keeps the struct (field
// `index soc`) in SSZ and delegates the non-template method bodies here:
//
//     lib sk = <socket>;
//     ...
//     public bool connect(^/char host, ^/char port, int timeout, bool nodelay)
//     {
//       ret .sk.socketConnect(`soc=, host, port, timeout, nodelay);
//     }
//
// The module is resolved by the native-lib registry (main/ssz/native_lib.hpp).
// The wrappers forward to the clean native operations in main/socket/socket.cpp
// (the same functions the static bridge wraps).  The templated
// recv<_t>/recvAry<_t>/send<_t>/sendAry<_t> methods and `accept` (which moves
// a whole `&Socket` out-param) stay in SSZ and keep calling the static
// `socket` plugin — both paths coexist.
// ============================================================================

#include <cstdint>
#include <string>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netdb.h>
#define INVALID_SOCKET -1
typedef int SOCKET;
#endif

#include "sszdef.h"
#include "native_lib.hpp"
#include "arrayandref.hpp"
#include "pluginutil.hpp"
#include "bridge.hpp"   // ikemen::ssz_bridge::refToNarrowUtf8

struct PluginUtil;

// Clean natives defined in main/socket/socket.cpp
extern void SSZ_STDCALL SocketClose(SOCKET* psoc);
extern bool SSZ_STDCALL SocketConnect(
	bool nodelay, int32_t timeout,
	const std::string& port, const std::string& host, SOCKET* psoc);
extern bool SSZ_STDCALL SocketListen(
	bool ipv4, int32_t backlog, const std::string& port, SOCKET* psoc);

// ---------------------------------------------------------------------------
// &Socket struct methods — the struct stays in SSZ (field `index soc`), the
// method bodies delegate here with the field passed as an `index=` out-param
// (pointer to the caller's slot, read-write).  Args arrive reversed vs. the
// SSZ declaration.
// ---------------------------------------------------------------------------

// SSZ: &Socket.setSoc(index s)
// Args arrive reversed: (pu, soc*, s).
static void SSZ_STDCALL SocketLibSetSoc(PluginUtil*, intptr_t* soc, intptr_t s)
{
	*soc = s;
}

// SSZ: &Socket.isOpen()
static bool SSZ_STDCALL SocketLibIsOpen(PluginUtil*, intptr_t soc)
{
	return soc != (intptr_t)INVALID_SOCKET;
}

// SSZ: &Socket.close() — socclose() also writes INVALID_SOCKET back.
static void SSZ_STDCALL SocketLibClose(PluginUtil*, intptr_t* soc)
{
	SocketClose((SOCKET*)soc);
}

// SSZ: &Socket.connect(^/char host, ^/char port, int timeout, bool nodelay)
// Args arrive reversed: (pu, nodelay, timeout, port, host, soc*).
static bool SSZ_STDCALL SocketLibConnect(
	PluginUtil* pu, bool nodelay, int32_t timeout,
	Reference port, Reference host, intptr_t* soc)
{
	return SocketConnect(
		nodelay, timeout,
		ikemen::ssz_bridge::refToNarrowUtf8(pu, port),
		ikemen::ssz_bridge::refToNarrowUtf8(pu, host),
		(SOCKET*)soc);
}

// SSZ: &Socket.listen(^/char port, int backlog, bool ipv4)
// Args arrive reversed: (pu, ipv4, backlog, port, soc*).
static bool SSZ_STDCALL SocketLibListen(
	PluginUtil* pu, bool ipv4, int32_t backlog, Reference port, intptr_t* soc)
{
	return SocketListen(
		ipv4, backlog,
		ikemen::ssz_bridge::refToNarrowUtf8(pu, port),
		(SOCKET*)soc);
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

extern "C" bool socket_lib_register()
{
	static const NativeLib::NativeFunction funcs[] = {
		{ "setSoc",        "void (index, index=)",         (void*)SocketLibSetSoc   },
		{ "isOpen",        "bool (index)",                 (void*)SocketLibIsOpen   },
		{ "socketClose",   "void (index=)",                (void*)SocketLibClose    },
		{ "socketConnect", "bool (index=, ^/char, ^/char, int, bool)", (void*)SocketLibConnect },
		{ "socketListen",  "bool (index=, ^/char, int, bool)",        (void*)SocketLibListen  },
	};

	NativeLib::NativeLibrary lib;
	lib.name = "socket";
	for(size_t i = 0; i < sizeof(funcs) / sizeof(funcs[0]); i++){
		lib.functions.push_back(funcs[i]);
	}
	return NativeLib::RegisterLibrary(lib);
}
