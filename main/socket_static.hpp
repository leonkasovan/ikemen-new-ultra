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

#ifdef _WIN32
#include <winsock2.h>
#else
typedef int SOCKET;
#endif

struct PluginUtil;
struct Reference;

// Initialises Winsock (idempotent).  Declared with C++ linkage to match its
// definition in socket.cpp — the extern "C" block below declares the bridge
// ABI, not this entry point.  It must be called from the executable: linked
// statically, the loader never calls DllMain, so the old DllMain-based
// WSAStartup never ran and every socket call failed with WSANOTINITIALISED.
bool SSZ_STDCALL SocketInit();

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
	// Winsock has to come up before the scripts can use the plugin.  A failure
	// here is reported but not fatal: the other 13 subsystems still work, and
	// skipping registration would turn a runtime socket failure into a
	// compile-time "not defined" error for scripts that reference it.
	if(!SocketInit()){
		LOG_INFO("Socket", "WSAStartup failed; socket plugin calls will not work");
	}

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
