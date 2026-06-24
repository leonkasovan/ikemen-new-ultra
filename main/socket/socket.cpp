
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#define WTOA(s) pu->refToAstr(CP_UTF8, s)
typedef int socklen_t;
#else
#include <netdb.h>
#include <netinet/tcp.h>
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define TRUE 1
#define SD_BOTH SHUT_RDWR
#define closesocket close
#define WTOA(s) pu->wToA(pu->refToWstr(s))
typedef int SOCKET;
typedef struct sockaddr SOCKADDR;
typedef struct addrinfo ADDRINFO;
#endif

#include "sszdef.h"

#include "typeid.h"
#include "arrayandref.hpp"
#include "pluginutil.hpp"


void socclose(SOCKET &soc)
{
	if(soc != INVALID_SOCKET){
		shutdown(soc, SD_BOTH);
		closesocket(soc);
		soc = INVALID_SOCKET;
	}
}
bool socconnect(
	SOCKET &soc, const char *host, const char *port,
	unsigned int timeout = 30, bool nodelay = false)
{
	socclose(soc);
	ADDRINFO Hints, *AddrInfo, *AI;
	memset(&Hints, 0, sizeof(Hints));
	Hints.ai_family = PF_UNSPEC;
	Hints.ai_socktype = SOCK_STREAM;
	if(getaddrinfo(host, port, &Hints, &AddrInfo)) return false;
	SOCKET s = INVALID_SOCKET;
	for(AI = AddrInfo; AI != NULL; AI = AI->ai_next){
		s = socket(AI->ai_family, AI->ai_socktype, AI->ai_protocol);
		if(s == INVALID_SOCKET) continue;
		if(connect(s, AI->ai_addr, (int)AI->ai_addrlen) != SOCKET_ERROR) break;
		closesocket(s);
		s = INVALID_SOCKET;
	}
	freeaddrinfo(AddrInfo);
	if(AI == NULL) return false;
	if(timeout > 0){
		timeout *= 1000;
		setsockopt(
			s, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout));
		setsockopt(
			s, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
	}
	if(nodelay){
		BOOL b = TRUE;
		setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (char*)&b, sizeof(b));
	}
	soc = s;
	return true;
}
bool soclisten(SOCKET &soc, const char *port, int backlog, bool ipv4)
{
	socclose(soc);
	ADDRINFO Hints, *AddrInfo, *AI;
	memset(&Hints, 0, sizeof(Hints));
	Hints.ai_family = ipv4 ? AF_INET : AF_UNSPEC;
	Hints.ai_socktype = SOCK_STREAM;
	Hints.ai_flags = AI_PASSIVE;
	if(getaddrinfo(NULL, port, &Hints, &AddrInfo)) return false;
	SOCKET s = INVALID_SOCKET;
	for(AI = AddrInfo; AI != NULL; AI = AI->ai_next){
		s = socket(AI->ai_family, AI->ai_socktype, AI->ai_protocol);
		if(s == INVALID_SOCKET) continue;
		if(
			bind(s, AI->ai_addr, (int)AI->ai_addrlen) != SOCKET_ERROR
			&& listen(s , backlog) != SOCKET_ERROR)
		{
			break;
		}
		closesocket(s);
		s = INVALID_SOCKET;
	}
	freeaddrinfo(AddrInfo);
	soc = s;
	return AI != NULL;
}
intptr_t socsend(SOCKET &soc, const char *buf, int len)
{
	intptr_t l = 0;
	if(len <= 0 || soc == INVALID_SOCKET) return l;
	l = send(soc, buf, len, 0);
	if(l < 0){
		socclose(soc);
		l = 0;
	}
	return l;
}
intptr_t socrecv(SOCKET &soc, char *buf, intptr_t len)
{
	intptr_t l = 0, tmp = 1;
	if(soc == INVALID_SOCKET) return l;
	while(len - l > 0 && (tmp = recv(soc, buf+l, (int)(len - l), 0)) > 0){
		l += tmp;
	}
	if(tmp <= 0) socclose(soc);
	return l;
}


#ifdef _WIN32
WSAData g_wdata;

BOOL WINAPI DllMain(HINSTANCE hinstDLL,DWORD fdwReason,LPVOID lpvReserved)
{
	switch(fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		WSAStartup(MAKEWORD(2,2), &g_wdata);
		break;
	case DLL_PROCESS_DETACH:
		WSACleanup();
		break;
	case DLL_THREAD_ATTACH:
		break;
	case DLL_THREAD_DETACH:
		break;
	}
	return TRUE;
}
#endif

extern "C" void SSZ_STDCALL SocketClose(PluginUtil* pu, SOCKET *psoc)
{
	socclose(*psoc);
}

extern "C" bool SSZ_STDCALL SocketConnect(PluginUtil* pu, bool nodelay, int32_t timeout,
	Reference port, Reference host, SOCKET *psoc)
{
	return
		socconnect(
			*psoc, WTOA(host).c_str(),
			WTOA(port).c_str(), timeout, nodelay);
}

extern "C" bool SSZ_STDCALL SocketListen(PluginUtil* pu, bool ipv4, int32_t backlog,
	Reference port, SOCKET *psoc)
{
	return
		soclisten(
			*psoc, WTOA(port).c_str(), backlog, ipv4);
}

extern "C" SOCKET SSZ_STDCALL SocketAccept(PluginUtil* pu, bool nodelay, int32_t timeout, SOCKET soc)
{
	if(soc == INVALID_SOCKET) return INVALID_SOCKET;
	fd_set readset;
	timeval to;
	SOCKADDR sa;
	socklen_t sasz = sizeof(sa);
	FD_ZERO(&readset);
	FD_SET(soc, &readset);
	to.tv_sec = timeout;
	to.tv_usec = 0;
	if(
		select((int)soc+1, &readset, NULL, NULL, &to) <= 0
		|| !FD_ISSET(soc, &readset))
	{
		return INVALID_SOCKET;
	}
	SOCKET s = accept(soc, &sa, &sasz);
	if(nodelay && s != INVALID_SOCKET){
		BOOL b = TRUE;
		setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (char*)&b, sizeof(b));
	}
	return s;
}

extern "C" bool SSZ_STDCALL SocketSend(PluginUtil* pu, intptr_t size, char *p, SOCKET *psoc)
{
	return socsend(*psoc, p, size) == size;
}

extern "C" intptr_t SSZ_STDCALL SocketSendAry(PluginUtil* pu, intptr_t size, Reference ary, SOCKET *psoc)
{
	if(ary.len() == 0) return 0;
	return socsend(*psoc, (char*)ary.atpos(), ary.len()) / size;
}

extern "C" bool SSZ_STDCALL SocketRecv(PluginUtil* pu, intptr_t size, char *p, SOCKET *psoc)
{
	return socrecv(*psoc, p, size) == size;
}

extern "C" intptr_t SSZ_STDCALL SocketRecvAry(PluginUtil* pu, intptr_t size, Reference ary, SOCKET *psoc)
{
	if(ary.len() == 0) return 0;
	return socrecv(*psoc, (char*)ary.atpos(), ary.len()) / size;
}
