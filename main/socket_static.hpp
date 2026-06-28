#pragma once
//
// socket_static.hpp
//
// Statically register every function exported by socket.cpp
// so that the SSZ runtime resolves them without loading socket.dll.
//
// IMPORTANT: On Windows, <winsock2.h> must be included BEFORE
//            <windows.h> (which sszdef.h pulls in).  Make sure
//            main.cpp includes <winsock2.h> before sszdef.h.
//

#include "static_plugin_registry.hpp"

#if IKEMEN_NATIVE_SOCKET_LIB

#ifdef _WIN32
#include <winsock2.h>
#else
typedef int SOCKET;
#endif

struct PluginUtil;
struct Reference;

extern "C"
{
	void     SSZ_STDCALL SocketClose(PluginUtil*, SOCKET*);
	bool     SSZ_STDCALL SocketConnect(PluginUtil*, bool, int32_t, Reference, Reference, SOCKET*);
	bool     SSZ_STDCALL SocketListen(PluginUtil*, bool, int32_t, Reference, SOCKET*);
	SOCKET   SSZ_STDCALL SocketAccept(PluginUtil*, bool, int32_t, SOCKET);
	bool     SSZ_STDCALL SocketSend(PluginUtil*, intptr_t, char*, SOCKET*);
	intptr_t SSZ_STDCALL SocketSendAry(PluginUtil*, intptr_t, Reference, SOCKET*);
	bool     SSZ_STDCALL SocketRecv(PluginUtil*, intptr_t, char*, SOCKET*);
	intptr_t SSZ_STDCALL SocketRecvAry(PluginUtil*, intptr_t, Reference, SOCKET*);
}

inline bool socket_static_register()
{
	static const SSZ_FunctionEntry socket_mapping[] =
	{
		{ "SocketClose",   (void*)SocketClose   },
		{ "SocketConnect", (void*)SocketConnect  },
		{ "SocketListen",  (void*)SocketListen   },
		{ "SocketAccept",  (void*)SocketAccept   },
		{ "SocketSend",    (void*)SocketSend     },
		{ "SocketSendAry", (void*)SocketSendAry  },
		{ "SocketRecv",    (void*)SocketRecv     },
		{ "SocketRecvAry", (void*)SocketRecvAry  },
	};

	return SSZ_RegisterFunction(
		"socket",
		socket_mapping,
		sizeof(socket_mapping) / sizeof(socket_mapping[0]));
}

#else
inline bool socket_static_register() { return true; }
#endif
