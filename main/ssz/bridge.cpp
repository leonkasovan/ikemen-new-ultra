// bridge.cpp — Old ABI wrappers for SSZ JIT compatibility
//
// Migration step 1: main/file/file.cpp functions have been converted to
// native C++ ABI. The SSZ runtime/static plugin table still calls the old
// ABI shape (PluginUtil* first argument, Reference strings), so this file
// provides compatibility wrappers that convert Reference -> native types
// and forward to the native implementations.
//
// As more main/*.cpp functions are converted, add their old-ABI wrappers here
// and keep each native implementation free of PluginUtil/Reference unless it
// truly owns SSZ VM memory.

#include "bridge.hpp"

#include <vector>
#include <cstdio>    // for FILE*
#include <SDL.h>       // for SDL_Rect, SDL_Surface, SDL_Color, SDL_Event
#include <SDL_ttf.h>   // for TTF_Font
#include "pluginutil.hpp"    // for SSZCALLBACK (used by LuaInit)

// -----------------------------------------------------------------------
// Helper: write std::vector<uint8_t> into a Reference as raw bytes.
// -----------------------------------------------------------------------
namespace ikemen::ssz_bridge {
static void vectorToRefBytes(
    const std::vector<uint8_t>& src,
    Reference* dst)
{
    dst->releaseanddelete();
    dst->refnew((intptr_t)src.size(), sizeof(int8_t));
    if (dst->len() > 0 && !src.empty())
        memcpy(dst->atpos(), src.data(), src.size());
}
}

// -----------------------------------------------------------------------
// Type forward-declared in main/socket/socket.cpp
// -----------------------------------------------------------------------
#ifndef _WIN32
typedef int SOCKET;
#endif

// -----------------------------------------------------------------------
// Native implementations from all main/*.cpp files
#include "ssz_native/plugin_native_api.hpp"
#include "ssz_native/ssz_trace.hpp"

// ---- Plugin-specific forward declarations used only by bridge.cpp ----
// These are here (not in plugin_native_api.hpp) because they depend on types
// (RNS, SSZCALLBACK, RegexMatchInfo, SOCKET, Client, OggVorbis, lua_State, etc.)
// that are only relevant in the bridge context.

#ifdef _WIN32
#include <regex>
#define RNS std
#else
#include <boost/regex.hpp>
#define RNS boost
#endif

typedef void* (SSZ_STDCALL* SSZCALLBACK)(void*, intptr_t, void*, intptr_t, intptr_t);
class Client;
class OggVorbis;
struct lua_State;
struct DynamicRef;

// Forward declaration for main Lua state access (defined in main/lua/lua.cpp)
struct lua_State;
lua_State* get_main_lua_state();

RNS::wregex* SSZ_STDCALL NewRegex(bool i, const std::wstring& ptn, std::wstring* error);
void         SSZ_STDCALL DeleteRegex(RNS::wregex* re);
std::vector<ikemen::ssz_bridge::RegexMatchInfo> SSZ_STDCALL RegexSearch(const std::wstring& str, RNS::wregex* re);
bool         SSZ_STDCALL ShellOpen(bool act, bool wait, const std::wstring& direct, const std::wstring& param, const std::wstring& file);
bool         SSZ_STDCALL MoveTrash(const std::wstring& file);
void         SSZ_STDCALL SocketClose(SOCKET* psoc);
bool         SSZ_STDCALL SocketConnect(bool nodelay, int32_t timeout, const std::string& port, const std::string& host, SOCKET* psoc);
bool         SSZ_STDCALL SocketListen(bool ipv4, int32_t backlog, const std::string& port, SOCKET* psoc);
SOCKET       SSZ_STDCALL SocketAccept(bool nodelay, int32_t timeout, SOCKET soc);
bool         SSZ_STDCALL SocketSend(intptr_t size, const char* p, SOCKET* psoc);
intptr_t     SSZ_STDCALL SocketSendAry(intptr_t size, const void* data, intptr_t bytes, SOCKET* psoc);
bool         SSZ_STDCALL SocketRecv(intptr_t size, char* p, SOCKET* psoc);
intptr_t     SSZ_STDCALL SocketRecvAry(intptr_t size, void* data, intptr_t bytes, SOCKET* psoc);
Client*      SSZ_STDCALL NewClient();
void         SSZ_STDCALL DeleteClient(Client* client);
bool         SSZ_STDCALL ClientStart(Client* client);
bool         SSZ_STDCALL ClientStop(Client* client);
bool         SSZ_STDCALL ClientBufferReady(Client* client);
bool         SSZ_STDCALL ClientSetBuffer(const float* buffer, intptr_t frames, Client* client);
bool         SSZ_STDCALL YesNo(const std::wstring& r);
void         SSZ_STDCALL VeryUnsafeCopy(intptr_t size, void* src, void* dst);
std::wstring SSZ_STDCALL GetClipboardStr();
intptr_t     SSZ_STDCALL TazyuuCheck(const std::wstring& name);
void         SSZ_STDCALL CloseTazyuuHandle(intptr_t mutex);
std::wstring SSZ_STDCALL GetInifileString(const std::wstring& def, const std::wstring& key, const std::wstring& app, const std::wstring& file);
int32_t      SSZ_STDCALL GetInifileInt(int32_t def, const std::wstring& key, const std::wstring& app, const std::wstring& file);
bool         SSZ_STDCALL WriteInifileString(const std::wstring& str, const std::wstring& key, const std::wstring& app, const std::wstring& file);
bool         SSZ_STDCALL UnCompress(const void* data, intptr_t bytes, std::vector<uint8_t>& output);
void         SSZ_STDCALL UbytesToStr(const void* data, intptr_t bytes, UINT cp, std::wstring& output);
void         SSZ_STDCALL StrToUbytes(const void* data, intptr_t bytes, UINT cp, std::vector<uint8_t>& output);
void         SSZ_STDCALL AsciiToLocal(const void* data, intptr_t bytes, std::wstring& output);
void         SSZ_STDCALL SetSharedString(const std::wstring& str);
std::wstring SSZ_STDCALL GetSharedString();
std::wstring SSZ_STDCALL InputStr(const std::wstring& title);
void         SSZ_STDCALL LuaInit(intptr_t refcopy, intptr_t refdest, SSZCALLBACK callback, void* handle);
lua_State*   SSZ_STDCALL NewState();
void         SSZ_STDCALL Close(lua_State* L);
bool         SSZ_STDCALL RunFile(const std::string& filename, lua_State* L);
bool         SSZ_STDCALL RunString(const std::string& s, lua_State* L);
int32_t      SSZ_STDCALL GetTop(lua_State* L);
void         SSZ_STDCALL GetGlobal(const std::string& var, lua_State* L);
void         SSZ_STDCALL Register(intptr_t func, const std::string& var, lua_State* L);
bool         SSZ_STDCALL Pcall(int32_t nresults, int32_t nargs, lua_State* L);
void         SSZ_STDCALL Pop(int32_t n, lua_State* L);
void         SSZ_STDCALL PushNumber(double n, lua_State* L);
bool         SSZ_STDCALL IsNumber(int32_t idx, lua_State* L);
double       SSZ_STDCALL ToNumber(int32_t idx, lua_State* L);
void         SSZ_STDCALL PushBoolean(bool b, lua_State* L);
bool         SSZ_STDCALL IsBoolean(int32_t idx, lua_State* L);
bool         SSZ_STDCALL ToBoolean(int32_t idx, lua_State* L);
void         SSZ_STDCALL PushString(const std::string& s, lua_State* L);
bool         SSZ_STDCALL IsString(int32_t idx, lua_State* L);
void         SSZ_STDCALL ToString(int32_t idx, lua_State* L, std::string& output);
void         SSZ_STDCALL PushRef(DynamicRef* userdata, lua_State* L);
void         SSZ_STDCALL ToRef(int32_t idx, DynamicRef* userdata, lua_State* L);
OggVorbis*   SSZ_STDCALL NewOggVorbis();
void         SSZ_STDCALL DeleteOggVorbis(OggVorbis* ov);
bool         SSZ_STDCALL OggVorbisOpen(const std::wstring& file, OggVorbis* ov);
void         SSZ_STDCALL OggVorbisClear(OggVorbis* ov);
int64_t      SSZ_STDCALL OggVorbisPcmTotal(OggVorbis* ov);
int32_t      SSZ_STDCALL OggVorbisChannels(OggVorbis* ov);
int32_t      SSZ_STDCALL OggVorbisRate(OggVorbis* ov);
intptr_t     SSZ_STDCALL OggVorbisRead(int16_t* buffer, intptr_t length, OggVorbis* ov);
int32_t      SSZ_STDCALL OggVorbisSeek(double time, OggVorbis* ov);

namespace ikemen::ssz_bridge {

// Helpers to convert a std::vector<std::wstring> into an SSZ Reference array.
// Each element becomes a Reference-sized slot in fls, with its own heap-allocated
// Reference containing the wide string data.
static void vectorToRefList(
    PluginUtil* pu,
    Reference* fls,
    const std::vector<std::wstring>& items)
{
    pu->setSSZFunc();
    fls->releaseanddelete();
    if (items.empty()) return;
    for (size_t i = 0; i < items.size(); i++)
    {
        intptr_t j = fls->addsize(1, sizeof(Reference), refzeroclearcb);
        ((Reference*)(fls->atpos() + j))->init();
        pu->wstrToRef(*(Reference*)(fls->atpos() + j), items[i]);
    }
}

} // namespace ikemen::ssz_bridge

// =========================================================================
// Bridge wrappers — old ABI -> native C++
// =========================================================================

extern "C" intptr_t SSZ_STDCALL Open(PluginUtil* pu, Reference md, Reference fn)
{
    SSZ_TRACE_CAT(TRACE_FILE, "Open");
    (void)pu;
    return Open(
ikemen::ssz_bridge::refToWstring(pu, md), ikemen::ssz_bridge::refToWstring(pu, fn));
}

extern "C" void SSZ_STDCALL FileClose(PluginUtil* pu, FILE *pFile)
{
    SSZ_TRACE_CAT(TRACE_FILE, "FileClose");
    (void)pu;
    FileClose(pFile);
}

extern "C" bool SSZ_STDCALL Read(PluginUtil* pu, intptr_t size, void *p, FILE *pFile)
{
    SSZ_TRACE_CAT(TRACE_FILE, "Read");
    (void)pu;
    return Read(size, p, pFile);
}

extern "C" intptr_t SSZ_STDCALL ReadAry(PluginUtil* pu, intptr_t size, Reference ary, FILE *pFile)
{
    SSZ_TRACE_CAT(TRACE_FILE, "ReadAry");
    (void)pu;
    return ReadAry(size, ary.atpos(), ary.len(), pFile);
}

extern "C" bool SSZ_STDCALL Write(PluginUtil* pu, intptr_t size, void *p, FILE *pFile)
{
    SSZ_TRACE_CAT(TRACE_FILE, "Write");
    (void)pu;
    return Write(size, p, pFile);
}

extern "C" intptr_t SSZ_STDCALL WriteAry(PluginUtil* pu, intptr_t size, Reference ary, FILE *pFile)
{
    SSZ_TRACE_CAT(TRACE_FILE, "WriteAry");
    (void)pu;
    return WriteAry(size, ary.atpos(), ary.len(), pFile);
}

extern "C" bool SSZ_STDCALL Seek(PluginUtil* pu, int32_t origin, int64_t offset, FILE *pFile)
{
    SSZ_TRACE_CAT(TRACE_FILE, "Seek");
    (void)pu;
    return Seek(origin, offset, pFile);
}

extern "C" void SSZ_STDCALL LoadAsciiText(PluginUtil* pu, Reference *pr, Reference r)
{
    SSZ_TRACE_CAT(TRACE_FILE, "LoadAsciiText");
    pu->setSSZFunc();
    std::wstring text = LoadAsciiText(
ikemen::ssz_bridge::refToWstring(pu, r));
    pr->releaseanddelete();
    if (text.empty()) return;
    pu->wstrToRef(*pr, text);
}

extern "C" bool SSZ_STDCALL SaveAsciiText(PluginUtil* pu, Reference txt, Reference r)
{
    SSZ_TRACE_CAT(TRACE_FILE, "SaveAsciiText");
    return SaveAsciiText(
        ikemen::ssz_bridge::refToWstring(pu, txt),
        ikemen::ssz_bridge::refToWstring(pu, r));
}

extern "C" bool SSZ_STDCALL Delete(PluginUtil* pu, Reference file)
{
    SSZ_TRACE_CAT(TRACE_FILE, "Delete");
    return Delete(ikemen::ssz_bridge::refToWstring(pu, file));
}

extern "C" bool SSZ_STDCALL Move(PluginUtil* pu, Reference newn, Reference oldn)
{
    SSZ_TRACE_CAT(TRACE_FILE, "Move");
    return Move(
        ikemen::ssz_bridge::refToWstring(pu, newn),
        ikemen::ssz_bridge::refToWstring(pu, oldn));
}

extern "C" bool SSZ_STDCALL Copy(PluginUtil* pu, bool overwrite, Reference dist, Reference source)
{
    SSZ_TRACE_CAT(TRACE_FILE, "Copy");
    return Copy(
        overwrite,
        ikemen::ssz_bridge::refToWstring(pu, dist),
        ikemen::ssz_bridge::refToWstring(pu, source));
}

extern "C" void SSZ_STDCALL Find(PluginUtil* pu, Reference *fls, Reference fn)
{
    SSZ_TRACE_CAT(TRACE_FILE, "Find");
    std::vector<std::wstring> files = Find(ikemen::ssz_bridge::refToWstring(pu, fn));
    ikemen::ssz_bridge::vectorToRefList(pu, fls, files);
}

extern "C" void SSZ_STDCALL FindDir(PluginUtil* pu, Reference *fls, Reference fn)
{
    SSZ_TRACE_CAT(TRACE_FILE, "FindDir");
    std::vector<std::wstring> dirs = FindDir(ikemen::ssz_bridge::refToWstring(pu, fn));
    ikemen::ssz_bridge::vectorToRefList(pu, fls, dirs);
}

extern "C" bool SSZ_STDCALL CreateDir(PluginUtil* pu, Reference dir)
{
    SSZ_TRACE_CAT(TRACE_FILE, "CreateDir");
    return CreateDir(ikemen::ssz_bridge::refToWstring(pu, dir));
}

extern "C" bool SSZ_STDCALL RemoveDir(PluginUtil* pu, Reference dir)
{
    SSZ_TRACE_CAT(TRACE_FILE, "RemoveDir");
    return RemoveDir(ikemen::ssz_bridge::refToWstring(pu, dir));
}

extern "C" bool SSZ_STDCALL SetCurrentDir(PluginUtil* pu, Reference dir)
{
    SSZ_TRACE_CAT(TRACE_FILE, "SetCurrentDir");
    return SetCurrentDir(ikemen::ssz_bridge::refToWstring(pu, dir));
}

extern "C" void SSZ_STDCALL GetCurrentDir(PluginUtil* pu, Reference* dir)
{
    SSZ_TRACE_CAT(TRACE_FILE, "GetCurrentDir");
    pu->setSSZFunc();
    std::wstring curdir = GetCurrentDir();
    dir->releaseanddelete();
    if (curdir.empty()) return;
    pu->wstrToRef(*dir, curdir);
}

// =========================================================================
// Socket wrappers — old ABI -> native C++
// =========================================================================

extern "C" void SSZ_STDCALL SocketClose(PluginUtil* pu, SOCKET *psoc)
{
    SSZ_TRACE_CAT(TRACE_NET, "SocketClose");
    (void)pu;
    SocketClose(psoc);
}

extern "C" bool SSZ_STDCALL SocketConnect(PluginUtil* pu, bool nodelay, int32_t timeout,
    Reference port, Reference host, SOCKET *psoc)
{
    SSZ_TRACE_CAT(TRACE_NET, "SocketConnect");
    return SocketConnect(
        nodelay, timeout,
        ikemen::ssz_bridge::refToNarrowUtf8(pu, port),
        ikemen::ssz_bridge::refToNarrowUtf8(pu, host),
        psoc);
}

extern "C" bool SSZ_STDCALL SocketListen(PluginUtil* pu, bool ipv4, int32_t backlog,
    Reference port, SOCKET *psoc)
{
    SSZ_TRACE_CAT(TRACE_NET, "SocketListen");
    return SocketListen(
        ipv4, backlog,
        ikemen::ssz_bridge::refToNarrowUtf8(pu, port),
        psoc);
}

extern "C" SOCKET SSZ_STDCALL SocketAccept(PluginUtil* pu, bool nodelay, int32_t timeout, SOCKET soc)
{
    SSZ_TRACE_CAT(TRACE_NET, "SocketAccept");
    (void)pu;
    return SocketAccept(nodelay, timeout, soc);
}

extern "C" bool SSZ_STDCALL SocketSend(PluginUtil* pu, intptr_t size, char *p, SOCKET *psoc)
{
    SSZ_TRACE_CAT(TRACE_NET, "SocketSend");
    (void)pu;
    return SocketSend(size, p, psoc);
}

extern "C" intptr_t SSZ_STDCALL SocketSendAry(PluginUtil* pu, intptr_t size, Reference ary, SOCKET *psoc)
{
    SSZ_TRACE_CAT(TRACE_NET, "SocketSendAry");
    (void)pu;
    return SocketSendAry(size, ary.atpos(), ary.len(), psoc);
}

extern "C" bool SSZ_STDCALL SocketRecv(PluginUtil* pu, intptr_t size, char *p, SOCKET *psoc)
{
    SSZ_TRACE_CAT(TRACE_NET, "SocketRecv");
    (void)pu;
    return SocketRecv(size, p, psoc);
}

extern "C" intptr_t SSZ_STDCALL SocketRecvAry(PluginUtil* pu, intptr_t size, Reference ary, SOCKET *psoc)
{
    SSZ_TRACE_CAT(TRACE_NET, "SocketRecvAry");
    (void)pu;
    return SocketRecvAry(size, ary.atpos(), ary.len(), psoc);
}

// =========================================================================
// Lua wrappers — old ABI -> native C++
// =========================================================================

extern "C" void SSZ_STDCALL LuaInit(PluginUtil* pu, intptr_t refcopy, intptr_t refdest)
{
    SSZ_TRACE_CAT(TRACE_LUA, "LuaInit");
    LuaInit(refcopy, refdest, pu->psf->callback, pu->handle);
}

extern "C" lua_State* SSZ_STDCALL NewState(PluginUtil* pu)
{
    SSZ_TRACE_CAT(TRACE_LUA, "NewState");
    (void)pu;
    return NewState();
}

extern "C" void SSZ_STDCALL Close(PluginUtil* pu, lua_State* L)
{
    SSZ_TRACE_CAT(TRACE_LUA, "Close");
    (void)pu;
    Close(L);
}

extern "C" bool SSZ_STDCALL RunFile(PluginUtil* pu, Reference filename, lua_State* L)
{
    SSZ_TRACE_CAT(TRACE_LUA, "RunFile");
    return RunFile(ikemen::ssz_bridge::refToNarrowUtf8(pu, filename), L);
}

extern "C" bool SSZ_STDCALL RunString(PluginUtil* pu, Reference s, lua_State* L)
{
    SSZ_TRACE_CAT(TRACE_LUA, "RunString");
    return RunString(ikemen::ssz_bridge::refToNarrowUtf8(pu, s), L);
}

extern "C" int32_t SSZ_STDCALL GetTop(PluginUtil* pu, lua_State* L)
{
    SSZ_TRACE_CAT(TRACE_LUA, "GetTop");
    (void)pu;
    return GetTop(L);
}

extern "C" void SSZ_STDCALL GetGlobal(PluginUtil* pu, Reference var, lua_State* L)
{
    SSZ_TRACE_CAT(TRACE_LUA, "GetGlobal");
    GetGlobal(ikemen::ssz_bridge::refToNarrowUtf8(pu, var), L);
}

extern "C" void SSZ_STDCALL Register(PluginUtil* pu, intptr_t func, Reference var, lua_State* L)
{
    SSZ_TRACE_CAT(TRACE_LUA, "Register");
    Register(func, ikemen::ssz_bridge::refToNarrowUtf8(pu, var), L);
}

extern "C" bool SSZ_STDCALL Pcall(PluginUtil* pu, int32_t nresults, int32_t nargs, lua_State* L)
{
    SSZ_TRACE_CAT(TRACE_LUA, "Pcall");
    (void)pu;
    return Pcall(nresults, nargs, L);
}

extern "C" void SSZ_STDCALL Pop(PluginUtil* pu, int32_t n, lua_State* L)
{
    SSZ_TRACE_CAT(TRACE_LUA, "Pop");
    (void)pu;
    Pop(n, L);
}

extern "C" void SSZ_STDCALL PushNumber(PluginUtil* pu, double n, lua_State* L)
{
    SSZ_TRACE_CAT(TRACE_LUA, "PushNumber");
    (void)pu;
    PushNumber(n, L);
}

extern "C" bool SSZ_STDCALL IsNumber(PluginUtil* pu, int32_t idx, lua_State* L)
{
    SSZ_TRACE_CAT(TRACE_LUA, "IsNumber");
    (void)pu;
    return IsNumber(idx, L);
}

extern "C" double SSZ_STDCALL ToNumber(PluginUtil* pu, int32_t idx, lua_State* L)
{
    SSZ_TRACE_CAT(TRACE_LUA, "ToNumber");
    (void)pu;
    return ToNumber(idx, L);
}

extern "C" void SSZ_STDCALL PushBoolean(PluginUtil* pu, bool b, lua_State* L)
{
    SSZ_TRACE_CAT(TRACE_LUA, "PushBoolean");
    (void)pu;
    PushBoolean(b, L);
}

extern "C" bool SSZ_STDCALL IsBoolean(PluginUtil* pu, int32_t idx, lua_State* L)
{
    SSZ_TRACE_CAT(TRACE_LUA, "IsBoolean");
    (void)pu;
    return IsBoolean(idx, L);
}

extern "C" bool SSZ_STDCALL ToBoolean(PluginUtil* pu, int32_t idx, lua_State* L)
{
    SSZ_TRACE_CAT(TRACE_LUA, "ToBoolean");
    (void)pu;
    return ToBoolean(idx, L);
}

extern "C" void SSZ_STDCALL PushString(PluginUtil* pu, Reference s, lua_State* L)
{
    SSZ_TRACE_CAT(TRACE_LUA, "PushString");
    PushString(ikemen::ssz_bridge::refToNarrowUtf8(pu, s), L);
}

extern "C" bool SSZ_STDCALL IsString(PluginUtil* pu, int32_t idx, lua_State* L)
{
    SSZ_TRACE_CAT(TRACE_LUA, "IsString");
    (void)pu;
    return IsString(idx, L);
}

extern "C" void SSZ_STDCALL ToString(PluginUtil* pu, int32_t idx, Reference* s, lua_State* L)
{
    SSZ_TRACE_CAT(TRACE_LUA, "ToString");
    pu->setSSZFunc();
    std::string output;
    ToString(idx, L, output);
    if (output.empty()) return;
    pu->astrToRef(CP_UTF8, *s, output);
}

extern "C" void SSZ_STDCALL PushRef(PluginUtil* pu, DynamicRef* userdata, lua_State* L)
{
    SSZ_TRACE_CAT(TRACE_LUA, "PushRef");
    (void)pu;
    PushRef(userdata, L);
}

extern "C" void SSZ_STDCALL ToRef(PluginUtil* pu, int32_t idx, DynamicRef* userdata, lua_State* L)
{
    SSZ_TRACE_CAT(TRACE_LUA, "ToRef");
    (void)pu;
    ToRef(idx, userdata, L);
}

// =========================================================================
// Mesdialog wrappers — old ABI -> native C++
// =========================================================================

extern "C" bool SSZ_STDCALL YesNo(PluginUtil* pu, Reference r)
{
    SSZ_TRACE_CAT(TRACE_UTIL, "YesNo");
    return YesNo(ikemen::ssz_bridge::refToWstring(pu, r));
}

extern "C" void SSZ_STDCALL VeryUnsafeCopy(PluginUtil* pu, intptr_t size, void *src, void *dst)
{
    SSZ_TRACE_CAT(TRACE_UTIL, "VeryUnsafeCopy");
    (void)pu;
    VeryUnsafeCopy(size, src, dst);
}

extern "C" bool SSZ_STDCALL GetClipboardStr(PluginUtil* pu, Reference *r)
{
    SSZ_TRACE_CAT(TRACE_UTIL, "GetClipboardStr");
    pu->setSSZFunc();
    std::wstring result = GetClipboardStr();
    if (result.empty()) return false;
    pu->wstrToRef(*r, result);
    return true;
}

extern "C" intptr_t SSZ_STDCALL TazyuuCheck(PluginUtil* pu, Reference name)
{
    SSZ_TRACE_CAT(TRACE_UTIL, "TazyuuCheck");
    return TazyuuCheck(ikemen::ssz_bridge::refToWstring(pu, name));
}

extern "C" void SSZ_STDCALL CloseTazyuuHandle(PluginUtil* pu, intptr_t mutex)
{
    SSZ_TRACE_CAT(TRACE_UTIL, "CloseTazyuuHandle");
    (void)pu;
    CloseTazyuuHandle(mutex);
}

extern "C" void SSZ_STDCALL GetInifileString(PluginUtil* pu, Reference* pstr,
    Reference def, Reference key, Reference app, Reference file)
{
    SSZ_TRACE_CAT(TRACE_UTIL, "GetInifileString");
    pu->setSSZFunc();
    std::wstring result = GetInifileString(
        ikemen::ssz_bridge::refToWstring(pu, def),
        ikemen::ssz_bridge::refToWstring(pu, key),
        ikemen::ssz_bridge::refToWstring(pu, app),
        ikemen::ssz_bridge::refToWstring(pu, file));
    pstr->releaseanddelete();
    pu->wstrToRef(*pstr, result);
}

extern "C" int32_t SSZ_STDCALL GetInifileInt(PluginUtil* pu, int32_t def,
    Reference key, Reference app, Reference file)
{
    SSZ_TRACE_CAT(TRACE_UTIL, "GetInifileInt");
    return GetInifileInt(
        def,
        ikemen::ssz_bridge::refToWstring(pu, key),
        ikemen::ssz_bridge::refToWstring(pu, app),
        ikemen::ssz_bridge::refToWstring(pu, file));
}

extern "C" bool SSZ_STDCALL WriteInifileString(PluginUtil* pu,
    Reference str, Reference key, Reference app, Reference file)
{
    SSZ_TRACE_CAT(TRACE_UTIL, "WriteInifileString");
    return WriteInifileString(
        ikemen::ssz_bridge::refToWstring(pu, str),
        ikemen::ssz_bridge::refToWstring(pu, key),
        ikemen::ssz_bridge::refToWstring(pu, app),
        ikemen::ssz_bridge::refToWstring(pu, file));
}

extern "C" bool SSZ_STDCALL UnCompress(PluginUtil* pu, Reference src, Reference *dst)
{
    SSZ_TRACE_CAT(TRACE_UTIL, "UnCompress");
    pu->setSSZFunc();
    std::vector<uint8_t> output;
    bool ok = UnCompress(src.atpos(), src.len(), output);
    dst->releaseanddelete();
    if (!ok) return false;
    ikemen::ssz_bridge::vectorToRefBytes(output, dst);
    return true;
}

extern "C" void SSZ_STDCALL UbytesToStr(PluginUtil* pu, Reference src, Reference *dst, UINT cp)
{
    SSZ_TRACE_CAT(TRACE_UTIL, "UbytesToStr");
    pu->setSSZFunc();
    dst->releaseanddelete();
    std::wstring output;
    UbytesToStr(src.atpos(), src.len(), cp, output);
    if (output.empty()) return;
    pu->wstrToRef(*dst, output);
}

extern "C" void SSZ_STDCALL StrToUbytes(PluginUtil* pu, Reference src, Reference *dst, UINT cp)
{
    SSZ_TRACE_CAT(TRACE_UTIL, "StrToUbytes");
    pu->setSSZFunc();
    dst->releaseanddelete();
    std::vector<uint8_t> output;
    StrToUbytes(src.atpos(), src.len(), cp, output);
    if (output.empty()) return;
    ikemen::ssz_bridge::vectorToRefBytes(output, dst);
}

extern "C" void SSZ_STDCALL AsciiToLocal(PluginUtil* pu, Reference src, Reference *dst)
{
    SSZ_TRACE_CAT(TRACE_UTIL, "AsciiToLocal");
    pu->setSSZFunc();
    dst->releaseanddelete();
    std::wstring output;
    AsciiToLocal(src.atpos(), src.len(), output);
    if (output.empty()) return;
    pu->wstrToRef(*dst, output);
}

extern "C" void SSZ_STDCALL SetSharedString(PluginUtil* pu, Reference str)
{
    SSZ_TRACE_CAT(TRACE_UTIL, "SetSharedString");
    SetSharedString(ikemen::ssz_bridge::refToWstring(pu, str));
}

extern "C" void SSZ_STDCALL GetSharedString(PluginUtil* pu, Reference *str)
{
    SSZ_TRACE_CAT(TRACE_UTIL, "GetSharedString");
    pu->setSSZFunc();
    std::wstring result = GetSharedString();
    str->releaseanddelete();
    pu->wstrToRef(*str, result);
}

extern "C" void SSZ_STDCALL InputStr(PluginUtil* pu, Reference *pr, Reference title)
{
    SSZ_TRACE_CAT(TRACE_UTIL, "InputStr");
    pu->setSSZFunc();
    std::wstring result = InputStr(ikemen::ssz_bridge::refToWstring(pu, title));
    pr->releaseanddelete();
    if (result.empty()) return;
    pu->wstrToRef(*pr, result);
}

// =========================================================================
// Sound wrappers — old ABI -> native C++
// =========================================================================

extern "C" Client* SSZ_STDCALL NewClient(PluginUtil* pu)
{
    SSZ_TRACE_CAT(TRACE_UTIL, "NewClient");
    (void)pu;
    return NewClient();
}

extern "C" void SSZ_STDCALL DeleteClient(PluginUtil* pu, Client* client)
{
    SSZ_TRACE_CAT(TRACE_UTIL, "DeleteClient");
    (void)pu;
    DeleteClient(client);
}

extern "C" bool SSZ_STDCALL ClientStart(PluginUtil* pu, Client* client)
{
    SSZ_TRACE_CAT(TRACE_UTIL, "ClientStart");
    (void)pu;
    return ClientStart(client);
}

extern "C" bool SSZ_STDCALL ClientStop(PluginUtil* pu, Client* client)
{
    SSZ_TRACE_CAT(TRACE_UTIL, "ClientStop");
    (void)pu;
    return ClientStop(client);
}

extern "C" bool SSZ_STDCALL ClientBufferReady(PluginUtil* pu, Client* client)
{
    SSZ_TRACE_CAT(TRACE_UTIL, "ClientBufferReady");
    (void)pu;
    return ClientBufferReady(client);
}

extern "C" bool SSZ_STDCALL ClientSetBuffer(PluginUtil* pu, Reference src, Client* client)
{
    SSZ_TRACE_CAT(TRACE_UTIL, "ClientSetBuffer");
    return ClientSetBuffer(
        (const float*)src.atpos(),
        src.len() / (intptr_t)sizeof(float),
        client);
}

// =========================================================================
// Ogg wrappers — old ABI -> native C++
// =========================================================================

extern "C" OggVorbis* SSZ_STDCALL NewOggVorbis(PluginUtil* pu)
{
    SSZ_TRACE_CAT(TRACE_OGG, "NewOggVorbis");
    (void)pu;
    return NewOggVorbis();
}

extern "C" void SSZ_STDCALL DeleteOggVorbis(PluginUtil* pu, OggVorbis* ov)
{
    SSZ_TRACE_CAT(TRACE_OGG, "DeleteOggVorbis");
    (void)pu;
    DeleteOggVorbis(ov);
}

extern "C" bool SSZ_STDCALL OggVorbisOpen(PluginUtil* pu, Reference file, OggVorbis* ov)
{
    SSZ_TRACE_CAT(TRACE_OGG, "OggVorbisOpen");
    return OggVorbisOpen(ikemen::ssz_bridge::refToWstring(pu, file), ov);
}

extern "C" void SSZ_STDCALL OggVorbisClear(PluginUtil* pu, OggVorbis* ov)
{
    SSZ_TRACE_CAT(TRACE_OGG, "OggVorbisClear");
    (void)pu;
    OggVorbisClear(ov);
}

extern "C" int64_t SSZ_STDCALL OggVorbisPcmTotal(PluginUtil* pu, OggVorbis* ov)
{
    SSZ_TRACE_CAT(TRACE_OGG, "OggVorbisPcmTotal");
    (void)pu;
    return OggVorbisPcmTotal(ov);
}

extern "C" int32_t SSZ_STDCALL OggVorbisChannels(PluginUtil* pu, OggVorbis* ov)
{
    SSZ_TRACE_CAT(TRACE_OGG, "OggVorbisChannels");
    (void)pu;
    return OggVorbisChannels(ov);
}

extern "C" int32_t SSZ_STDCALL OggVorbisRate(PluginUtil* pu, OggVorbis* ov)
{
    SSZ_TRACE_CAT(TRACE_OGG, "OggVorbisRate");
    (void)pu;
    return OggVorbisRate(ov);
}

extern "C" intptr_t SSZ_STDCALL OggVorbisRead(PluginUtil* pu, Reference buffer, OggVorbis* ov)
{
    SSZ_TRACE_CAT(TRACE_OGG, "OggVorbisRead");
    (void)pu;
    return OggVorbisRead((int16_t*)buffer.atpos(), (intptr_t)(buffer.len() / sizeof(int16_t)), ov);
}

extern "C" int32_t SSZ_STDCALL OggVorbisSeek(PluginUtil* pu, double time, OggVorbis* ov)
{
    SSZ_TRACE_CAT(TRACE_OGG, "OggVorbisSeek");
    (void)pu;
    return OggVorbisSeek(time, ov);
}

// =========================================================================
// Regex wrappers — old ABI -> native C++
// =========================================================================

extern "C" RNS::wregex* SSZ_STDCALL NewRegex(PluginUtil* pu, Reference* error, bool i, Reference ptn)
{
    SSZ_TRACE_CAT(TRACE_UTIL, "NewRegex");
    pu->setSSZFunc();
    error->releaseanddelete();
    std::wstring errorStr;
    RNS::wregex* re = NewRegex(i, ikemen::ssz_bridge::refToWstring(pu, ptn), &errorStr);
    if (!errorStr.empty())
    {
        pu->wstrToRef(*error, errorStr);
        delete re;
        re = nullptr;
    }
    return re;
}

extern "C" void SSZ_STDCALL DeleteRegex(PluginUtil* pu, RNS::wregex* re)
{
    SSZ_TRACE_CAT(TRACE_UTIL, "DeleteRegex");
    (void)pu;
    DeleteRegex(re);
}

extern "C" void SSZ_STDCALL RegexSearch(PluginUtil* pu, Reference* matches, Reference str, RNS::wregex* re)
{
    SSZ_TRACE_CAT(TRACE_UTIL, "RegexSearch");
    pu->setSSZFunc();
    matches->releaseanddelete();
    if(!re) return;

    std::vector<ikemen::ssz_bridge::RegexMatchInfo> result =
        RegexSearch(ikemen::ssz_bridge::refToWstring(pu, str), re);
    if (result.empty()) return;

    matches->refnew((intptr_t)result.size(), sizeof(Reference));
    for (size_t i = 0; i < result.size(); i++)
    {
        auto& m = result[i];
        ((Reference*)matches->atpos())[i].init();
        if (m.len > 0)
        {
            ((Reference*)matches->atpos())[i].copy(str);
            ((Reference*)matches->atpos())[i].position += m.pos * sizeof(WCHR);
            ((Reference*)matches->atpos())[i].length = m.len * sizeof(WCHR);
        }
        else
        {
            ((Reference*)matches->atpos())[i].position =
                (m.pos != -1 ? str.pos() + m.pos : -1) * sizeof(WCHR);
        }
    }
}

// =========================================================================
// Shell wrappers — old ABI -> native C++
// =========================================================================

extern "C" bool SSZ_STDCALL ShellOpen(PluginUtil* pu, bool act, bool wait, Reference direct, Reference param, Reference file)
{
    SSZ_TRACE_CAT(TRACE_UTIL, "ShellOpen");
    return ShellOpen(
        act, wait,
        ikemen::ssz_bridge::refToWstring(pu, direct),
        ikemen::ssz_bridge::refToWstring(pu, param),
        ikemen::ssz_bridge::refToWstring(pu, file));
}

extern "C" bool SSZ_STDCALL MoveTrash(PluginUtil* pu, Reference file)
{
    SSZ_TRACE_CAT(TRACE_UTIL, "MoveTrash");
    return MoveTrash(ikemen::ssz_bridge::refToWstring(pu, file));
}

// =========================================================================
// Thread wrappers — old ABI -> native C++
// =========================================================================

extern "C" void SSZ_STDCALL ThreadDelay(PluginUtil* pu, uint32_t ui)
{
    SSZ_TRACE_CAT(TRACE_UTIL, "ThreadDelay");
    (void)pu;
    ThreadDelay(ui);
}

// =========================================================================
// Alert wrappers — old ABI -> native C++
// =========================================================================

extern "C" void SSZ_STDCALL Alert(PluginUtil* pu, Reference title, Reference mes)
{
    SSZ_TRACE_CAT(TRACE_UTIL, "Alert");
    Alert(
        ikemen::ssz_bridge::refToWstring(pu, title),
        ikemen::ssz_bridge::refToWstring(pu, mes));
}

// =========================================================================
// Time wrappers — old ABI -> native C++
// =========================================================================

extern "C" uint32_t SSZ_STDCALL TickCount(PluginUtil* pu)
{
    SSZ_TRACE_CAT(TRACE_MATH, "TickCount");
    (void)pu;
    return TickCount();
}

extern "C" int64_t SSZ_STDCALL UnixTime(PluginUtil* pu)
{
    SSZ_TRACE_CAT(TRACE_MATH, "UnixTime");
    (void)pu;
    return UnixTime();
}

// =========================================================================
// Math wrappers — old ABI -> native C++
// =========================================================================

extern "C" double SSZ_STDCALL Sin(PluginUtil* pu, double x)
{
    SSZ_TRACE_CAT(TRACE_MATH, "Sin");
    (void)pu;
    return Sin(x);
}

extern "C" double SSZ_STDCALL Cos(PluginUtil* pu, double x)
{
    SSZ_TRACE_CAT(TRACE_MATH, "Cos");
    (void)pu;
    return Cos(x);
}

extern "C" double SSZ_STDCALL Tan(PluginUtil* pu, double x)
{
    SSZ_TRACE_CAT(TRACE_MATH, "Tan");
    (void)pu;
    return Tan(x);
}

extern "C" double SSZ_STDCALL ASin(PluginUtil* pu, double x)
{
    SSZ_TRACE_CAT(TRACE_MATH, "ASin");
    (void)pu;
    return ASin(x);
}

extern "C" double SSZ_STDCALL ACos(PluginUtil* pu, double x)
{
    SSZ_TRACE_CAT(TRACE_MATH, "ACos");
    (void)pu;
    return ACos(x);
}

extern "C" double SSZ_STDCALL ATan(PluginUtil* pu, double x)
{
    SSZ_TRACE_CAT(TRACE_MATH, "ATan");
    (void)pu;
    return ATan(x);
}

extern "C" double SSZ_STDCALL Log(PluginUtil* pu, double y, double x)
{
    SSZ_TRACE_CAT(TRACE_MATH, "Log");
    (void)pu;
    return Log(y, x);
}

extern "C" double SSZ_STDCALL Ln(PluginUtil* pu, double x)
{
    SSZ_TRACE_CAT(TRACE_MATH, "Ln");
    (void)pu;
    return Ln(x);
}

extern "C" double SSZ_STDCALL Exp(PluginUtil* pu, double x)
{
    SSZ_TRACE_CAT(TRACE_MATH, "Exp");
    (void)pu;
    return Exp(x);
}

extern "C" double SSZ_STDCALL Sqrt(PluginUtil* pu, double x)
{
    SSZ_TRACE_CAT(TRACE_MATH, "Sqrt");
    (void)pu;
    return Sqrt(x);
}

extern "C" double SSZ_STDCALL Ceil(PluginUtil* pu, double x)
{
    SSZ_TRACE_CAT(TRACE_MATH, "Ceil");
    (void)pu;
    return Ceil(x);
}

extern "C" double SSZ_STDCALL Floor(PluginUtil* pu, double x)
{
    SSZ_TRACE_CAT(TRACE_MATH, "Floor");
    (void)pu;
    return Floor(x);
}

extern "C" bool SSZ_STDCALL IsFinite(PluginUtil* pu, double x)
{
    SSZ_TRACE_CAT(TRACE_MATH, "IsFinite");
    (void)pu;
    return IsFinite(x);
}

extern "C" bool SSZ_STDCALL IsInf(PluginUtil* pu, double x)
{
    SSZ_TRACE_CAT(TRACE_MATH, "IsInf");
    (void)pu;
    return IsInf(x);
}

extern "C" bool SSZ_STDCALL IsNaN(PluginUtil* pu, double x)
{
    SSZ_TRACE_CAT(TRACE_MATH, "IsNaN");
    (void)pu;
    return IsNaN(x);
}

// =========================================================================
// SDL plugin wrappers — old ABI -> native C++
// =========================================================================

// Forward declarations for native functions defined in main/sdlplugin/sdlplugin.cpp
void       SSZ_STDCALL DrawTTF(int32_t alpha, int32_t b, int32_t g, int32_t r, float scaleY, float scaleX, int32_t y, int32_t x, const std::wstring& text, int32_t align, const std::wstring& fontPath);
bool       SSZ_STDCALL Init(bool mugen, int32_t h, int32_t w, const std::wstring& cap);
bool       SSZ_STDCALL GlInit(int32_t h, int32_t w, const std::wstring& cap);
bool       SSZ_STDCALL RendererInit(const std::wstring& rendererName, int32_t h, int32_t w, const std::wstring& cap);
void       SSZ_STDCALL GetRendererInfo();
void       SSZ_STDCALL EnablePerfMonitor(bool enable);
void       SSZ_STDCALL End();
void       SSZ_STDCALL FullScreenExclusive(bool fsr);
bool       SSZ_STDCALL FullScreen(bool fs);
void       SSZ_STDCALL WindowType(int state);
int        SSZ_STDCALL GetWidth();
int        SSZ_STDCALL GetHeight();
void       SSZ_STDCALL WindowSize(int height, int width);
void       SSZ_STDCALL AspectRatio(bool aspect);
void       SSZ_STDCALL SetOpacity(float wo);
void       SSZ_STDCALL TakeScreenShot(const std::wstring& dir);
bool       SSZ_STDCALL UpdateGLViewport(const SDL_Event& event);
int        SSZ_STDCALL PlayVideo(const std::wstring& fn, const std::wstring& screenshotPath, int volume, int audioTrack);
bool       SSZ_STDCALL PollEvent(int8_t* pb);
char16_t   SSZ_STDCALL GetLastChar();
bool       SSZ_STDCALL KeyState(int32_t key);
bool       SSZ_STDCALL JoystickButtonState(int32_t btn, int32_t joy);
int32_t    SSZ_STDCALL PollInputBitmask(int32_t jn, int32_t u, int32_t d, int32_t l, int32_t r, int32_t a, int32_t b, int32_t c, int32_t x, int32_t y, int32_t z, int32_t q, int32_t w, int32_t e, int32_t s, int32_t jn2, int32_t u2, int32_t d2, int32_t l2, int32_t r2, int32_t a2, int32_t b2, int32_t c2, int32_t x2, int32_t y2, int32_t z2, int32_t q2, int32_t w2, int32_t e2, int32_t s2, int32_t sec);
void       SSZ_STDCALL SoftFill(uint32_t color, SDL_Rect* prect);
void       SSZ_STDCALL Fill(uint32_t color, SDL_Rect* prect);
intptr_t   SSZ_STDCALL IMGLoad(const std::wstring& fn);
void       SSZ_STDCALL DecodePNG8(FILE* fp, int32_t* h, int32_t* w, std::vector<uint8_t>& out);
void       SSZ_STDCALL BlitSurface(SDL_Rect* prect, SDL_Surface* psrcs);
intptr_t   SSZ_STDCALL CreatePaletteSurface(int32_t h, int32_t w, SDL_Color* ppl, uint8_t* ppx);
void       SSZ_STDCALL SetColorKey(uint32_t key, SDL_Surface* psur);
void       SSZ_STDCALL Flip();
intptr_t   SSZ_STDCALL AllocSurface(int32_t h, int32_t w);
void       SSZ_STDCALL FreeSurface(SDL_Surface* ps);
void       SSZ_STDCALL Delay(uint32_t ms);
uint32_t   SSZ_STDCALL GetTicks();
void       SSZ_STDCALL CursorShow(bool show);
intptr_t   SSZ_STDCALL OpenFont(int32_t size, const std::wstring& font);
void       SSZ_STDCALL CloseFont(TTF_Font* pf);
void       SSZ_STDCALL RenderFont(const std::wstring& str, int32_t y, int32_t x, SDL_Color c, TTF_Font* pf);
bool       SSZ_STDCALL SetSndBuf(int32_t* buf);
bool       SSZ_STDCALL PlayBGM(const std::wstring& fn, const std::wstring& pldir);
void       SSZ_STDCALL PauseBGM(bool pause);
bool       SSZ_STDCALL SendOpenBGM(int32_t channels, int32_t rate);
void       SSZ_STDCALL SendCloseBGM();
intptr_t   SSZ_STDCALL SendWriteBGM();
void       SSZ_STDCALL SetVolume(float bv, float wv, float gv);
void       SSZ_STDCALL FadeInBGM(int time);
void       SSZ_STDCALL FadeOutBGM(int time);
bool       SSZ_STDCALL RenderMugenZoom(Reference* pluginbuf, int32_t rle, float rcy, float rcx, SDL_Rect* pdstr, int32_t alpha, uint32_t roto, float rasterxadd, float yscl, float xbotscl, float xtopscl, SDL_Rect* tile, float ty, float cx, SDL_Rect* psrcr, uint16_t ckey, uint32_t* ppal, Reference img);
bool       SSZ_STDCALL RenderFontBatch(int32_t count, int32_t* glyphData, float spacing, float yscl, float xscl, SDL_Rect* window, int32_t alpha, int32_t glyphH, int32_t atlasStride, uint32_t* ppal, float baseY, float baseX, uint8_t* atlasPixels);
bool       SSZ_STDCALL RenderMugenShadow(Reference* pluginbuf, int32_t rle, float rcy, float rcx, SDL_Rect* pdstr, int32_t alpha, uint32_t roto, float vscl, float yscl, float xscl, float ty, float cx, SDL_Rect* psrcr, uint32_t color, Reference img);
uint32_t   SSZ_STDCALL Load8bitTexture(int32_t h, int32_t w, uint8_t* ppxl);
uint32_t   SSZ_STDCALL LoadPngTexture(FILE* fp, int32_t* h, int32_t* w);
void       SSZ_STDCALL DeleteGlTexture(uint32_t texid);
void       SSZ_STDCALL GlSwapBuffers();
bool       SSZ_STDCALL InitMugenGl();
bool       SSZ_STDCALL RenderMugenGl(float rcy, float rcx, SDL_Rect* dstr, int alpha, float angle, float rasterxadd, float vscl, float yscl, float xbotscl, float xtopscl, SDL_Rect* tile, float y, float x, SDL_Rect* rect, int mask, uint8_t* ppal, uint32_t texid);
bool       SSZ_STDCALL RenderMugenGlFc(float mulb, float mulg, float mulr, float addb, float addg, float addr, float color, bool neg, float rcy, float rcx, SDL_Rect* dstr, int alpha, float angle, float rasterxadd, float vscl, float yscl, float xbotscl, float xtopscl, SDL_Rect* tile, float y, float x, SDL_Rect* rect, uint32_t texid);
bool       SSZ_STDCALL RenderMugenGlFcS(uint32_t color, float rcy, float rcx, SDL_Rect* dstr, int alpha, float angle, float rasterxadd, float vscl, float yscl, float xbotscl, float xtopscl, SDL_Rect* tile, float y, float x, SDL_Rect* rect, uint32_t texid);
void       SSZ_STDCALL MugenFillGl(int32_t alpha, uint32_t color, SDL_Rect rect);
bool       SSZ_STDCALL BindGlContext();
bool       SSZ_STDCALL UnbindGlContext();

extern "C" void SSZ_STDCALL DrawTTF(PluginUtil* pu, int32_t alpha, int32_t b, int32_t g, int32_t r, float scaleY, float scaleX, int32_t y, int32_t x, Reference text, int32_t align, Reference fontPath)
{
    SSZ_TRACE_CAT(TRACE_SDL, "DrawTTF");
    DrawTTF(
        alpha, b, g, r, scaleY, scaleX, y, x,
        ikemen::ssz_bridge::refToWstring(pu, text),
        align,
        ikemen::ssz_bridge::refToWstring(pu, fontPath));
}

extern "C" bool SSZ_STDCALL Init(PluginUtil* pu, bool mugen, int32_t h, int32_t w, Reference cap)
{
    SSZ_TRACE_CAT(TRACE_SDL, "Init");
    return Init(mugen, h, w, ikemen::ssz_bridge::refToWstring(pu, cap));
}

extern "C" bool SSZ_STDCALL GlInit(PluginUtil* pu, int32_t h, int32_t w, Reference cap)
{
    SSZ_TRACE_CAT(TRACE_SDL, "GlInit");
    return GlInit(h, w, ikemen::ssz_bridge::refToWstring(pu, cap));
}

extern "C" bool SSZ_STDCALL RendererInit(PluginUtil* pu, Reference rendererName, int32_t h, int32_t w, Reference cap)
{
    SSZ_TRACE_CAT(TRACE_SDL, "RendererInit");
    return RendererInit(
        ikemen::ssz_bridge::refToWstring(pu, rendererName),
        h, w,
        ikemen::ssz_bridge::refToWstring(pu, cap));
}

extern "C" void SSZ_STDCALL GetRendererInfo(PluginUtil* pu, Reference* outInfo)
{
    SSZ_TRACE_CAT(TRACE_SDL, "GetRendererInfo");
    (void)pu;
    (void)outInfo;
    GetRendererInfo();
    // TODO: Populate outInfo with actual renderer info (backend, device, driver).
    // Currently a stub — the SSZ script expects an output Reference but the
    // native GetRendererInfo() is void and logs to console instead.
}

extern "C" void SSZ_STDCALL EnablePerfMonitor(PluginUtil* pu, bool enable)
{
    SSZ_TRACE_CAT(TRACE_SDL, "EnablePerfMonitor");
    (void)pu;
    EnablePerfMonitor(enable);
}

extern "C" void SSZ_STDCALL End(PluginUtil* pu)
{
    SSZ_TRACE_CAT(TRACE_SDL, "End");
    (void)pu;
    End();
}

extern "C" void SSZ_STDCALL FullScreenExclusive(PluginUtil* pu, bool fsr)
{
    SSZ_TRACE_CAT(TRACE_SDL, "FullScreenExclusive");
    (void)pu;
    FullScreenExclusive(fsr);
}

extern "C" bool SSZ_STDCALL FullScreen(PluginUtil* pu, bool fs)
{
    SSZ_TRACE_CAT(TRACE_SDL, "FullScreen");
    (void)pu;
    return FullScreen(fs);
}

extern "C" void SSZ_STDCALL WindowType(PluginUtil* pu, int state)
{
    SSZ_TRACE_CAT(TRACE_SDL, "WindowType");
    (void)pu;
    WindowType(state);
}

extern "C" int SSZ_STDCALL GetWidth(PluginUtil* pu)
{
    SSZ_TRACE_CAT(TRACE_SDL, "GetWidth");
    (void)pu;
    return GetWidth();
}

extern "C" int SSZ_STDCALL GetHeight(PluginUtil* pu)
{
    SSZ_TRACE_CAT(TRACE_SDL, "GetHeight");
    (void)pu;
    return GetHeight();
}

extern "C" void SSZ_STDCALL WindowSize(PluginUtil* pu, int height, int width)
{
    SSZ_TRACE_CAT(TRACE_SDL, "WindowSize");
    (void)pu;
    WindowSize(height, width);
}

extern "C" void SSZ_STDCALL AspectRatio(PluginUtil* pu, bool aspect)
{
    SSZ_TRACE_CAT(TRACE_SDL, "AspectRatio");
    (void)pu;
    AspectRatio(aspect);
}

extern "C" void SSZ_STDCALL SetOpacity(PluginUtil* pu, float wo)
{
    SSZ_TRACE_CAT(TRACE_SDL, "SetOpacity");
    (void)pu;
    SetOpacity(wo);
}

extern "C" void SSZ_STDCALL TakeScreenShot(PluginUtil* pu, Reference dir)
{
    SSZ_TRACE_CAT(TRACE_SDL, "TakeScreenShot");
    TakeScreenShot(ikemen::ssz_bridge::refToWstring(pu, dir));
}

extern "C" bool SSZ_STDCALL UpdateGLViewport(PluginUtil* pu, const SDL_Event& event)
{
    SSZ_TRACE_CAT(TRACE_SDL, "UpdateGLViewport");
    (void)pu;
    return UpdateGLViewport(event);
}

extern "C" int SSZ_STDCALL PlayVideo(PluginUtil* pu, Reference fn, Reference screenshotPath, int volume, int audioTrack)
{
    SSZ_TRACE_CAT(TRACE_SDL, "PlayVideo");
    return PlayVideo(
        ikemen::ssz_bridge::refToWstring(pu, fn),
        ikemen::ssz_bridge::refToWstring(pu, screenshotPath),
        volume, audioTrack);
}

extern "C" bool SSZ_STDCALL PollEvent(PluginUtil* pu, int8_t* pb)
{
    SSZ_TRACE_CAT(TRACE_SDL, "PollEvent");
    (void)pu;
    return PollEvent(pb);
}

extern "C" char16_t SSZ_STDCALL GetLastChar(PluginUtil* pu)
{
    SSZ_TRACE_CAT(TRACE_SDL, "GetLastChar");
    (void)pu;
    return GetLastChar();
}

extern "C" bool SSZ_STDCALL KeyState(PluginUtil* pu, int32_t key)
{
    SSZ_TRACE_CAT(TRACE_SDL, "KeyState");
    (void)pu;
    return KeyState(key);
}

extern "C" bool SSZ_STDCALL JoystickButtonState(PluginUtil* pu, int32_t btn, int32_t joy)
{
    SSZ_TRACE_CAT(TRACE_SDL, "JoystickButtonState");
    (void)pu;
    return JoystickButtonState(btn, joy);
}

extern "C" int32_t SSZ_STDCALL PollInputBitmask(PluginUtil* pu,
    int32_t jn,
    int32_t u,  int32_t d,  int32_t l,  int32_t r,
    int32_t a,  int32_t b,  int32_t c,
    int32_t x,  int32_t y,  int32_t z,
    int32_t q,  int32_t w,  int32_t e,  int32_t s,
    int32_t jn2,
    int32_t u2, int32_t d2, int32_t l2, int32_t r2,
    int32_t a2, int32_t b2, int32_t c2,
    int32_t x2, int32_t y2, int32_t z2,
    int32_t q2, int32_t w2, int32_t e2, int32_t s2,
    int32_t sec)
{
    SSZ_TRACE_CAT(TRACE_SDL, "PollInputBitmask");
    (void)pu;
    return PollInputBitmask(
        jn, u, d, l, r, a, b, c, x, y, z, q, w, e, s,
        jn2, u2, d2, l2, r2, a2, b2, c2, x2, y2, z2, q2, w2, e2, s2,
        sec);
}

extern "C" void SSZ_STDCALL SoftFill(PluginUtil* pu, uint32_t color, SDL_Rect* prect)
{
    SSZ_TRACE_CAT(TRACE_SDL, "SoftFill");
    (void)pu;
    SoftFill(color, prect);
}

extern "C" void SSZ_STDCALL Fill(PluginUtil* pu, uint32_t color, SDL_Rect* prect)
{
    SSZ_TRACE_CAT(TRACE_SDL, "Fill");
    (void)pu;
    Fill(color, prect);
}

extern "C" intptr_t SSZ_STDCALL IMGLoad(PluginUtil* pu, Reference fn)
{
    SSZ_TRACE_CAT(TRACE_SDL, "IMGLoad");
    return IMGLoad(ikemen::ssz_bridge::refToWstring(pu, fn));
}

extern "C" void SSZ_STDCALL DecodePNG8(PluginUtil* pu, FILE* fp, int32_t* h, int32_t* w, Reference* out)
{
    SSZ_TRACE_CAT(TRACE_SDL, "DecodePNG8");
    pu->setSSZFunc();
    std::vector<uint8_t> decoded;
    DecodePNG8(fp, h, w, decoded);
    ikemen::ssz_bridge::vectorToRefBytes(decoded, out);
}

extern "C" void SSZ_STDCALL BlitSurface(PluginUtil* pu, SDL_Rect* prect, SDL_Surface* psrcs)
{
    SSZ_TRACE_CAT(TRACE_SDL, "BlitSurface");
    (void)pu;
    BlitSurface(prect, psrcs);
}

extern "C" intptr_t SSZ_STDCALL CreatePaletteSurface(PluginUtil* pu, int32_t h, int32_t w, SDL_Color* ppl, uint8_t* ppx)
{
    SSZ_TRACE_CAT(TRACE_SDL, "CreatePaletteSurface");
    (void)pu;
    return CreatePaletteSurface(h, w, ppl, ppx);
}

extern "C" void SSZ_STDCALL SetColorKey(PluginUtil* pu, uint32_t key, SDL_Surface* psur)
{
    SSZ_TRACE_CAT(TRACE_SDL, "SetColorKey");
    (void)pu;
    SetColorKey(key, psur);
}

extern "C" void SSZ_STDCALL Flip(PluginUtil* pu)
{
    SSZ_TRACE_CAT(TRACE_SDL, "Flip");
    (void)pu;
    Flip();
}

extern "C" intptr_t SSZ_STDCALL AllocSurface(PluginUtil* pu, int32_t h, int32_t w)
{
    SSZ_TRACE_CAT(TRACE_SDL, "AllocSurface");
    (void)pu;
    return AllocSurface(h, w);
}

extern "C" void SSZ_STDCALL FreeSurface(PluginUtil* pu, SDL_Surface* ps)
{
    SSZ_TRACE_CAT(TRACE_SDL, "FreeSurface");
    (void)pu;
    FreeSurface(ps);
}

extern "C" void SSZ_STDCALL Delay(PluginUtil* pu, uint32_t ms)
{
    SSZ_TRACE_CAT(TRACE_SDL, "Delay");
    (void)pu;
    Delay(ms);
}

extern "C" uint32_t SSZ_STDCALL GetTicks(PluginUtil* pu)
{
    SSZ_TRACE_CAT(TRACE_SDL, "GetTicks");
    (void)pu;
    return GetTicks();
}

extern "C" void SSZ_STDCALL CursorShow(PluginUtil* pu, bool show)
{
    SSZ_TRACE_CAT(TRACE_SDL, "CursorShow");
    (void)pu;
    CursorShow(show);
}

extern "C" intptr_t SSZ_STDCALL OpenFont(PluginUtil* pu, int32_t size, Reference font)
{
    SSZ_TRACE_CAT(TRACE_SDL, "OpenFont");
    return OpenFont(size, ikemen::ssz_bridge::refToWstring(pu, font));
}

extern "C" void SSZ_STDCALL CloseFont(PluginUtil* pu, TTF_Font* pf)
{
    SSZ_TRACE_CAT(TRACE_SDL, "CloseFont");
    (void)pu;
    CloseFont(pf);
}

extern "C" void SSZ_STDCALL RenderFont(PluginUtil* pu, Reference str, int32_t y, int32_t x, SDL_Color c, TTF_Font* pf)
{
    SSZ_TRACE_CAT(TRACE_SDL, "RenderFont");
    RenderFont(ikemen::ssz_bridge::refToWstring(pu, str), y, x, c, pf);
}

extern "C" bool SSZ_STDCALL SetSndBuf(PluginUtil* pu, int32_t* buf)
{
    SSZ_TRACE_CAT(TRACE_SDL, "SetSndBuf");
    (void)pu;
    return SetSndBuf(buf);
}

extern "C" bool SSZ_STDCALL PlayBGM(PluginUtil* pu, Reference fn, Reference pldir)
{
    SSZ_TRACE_CAT(TRACE_SDL, "PlayBGM");
    return PlayBGM(
        ikemen::ssz_bridge::refToWstring(pu, fn),
        ikemen::ssz_bridge::refToWstring(pu, pldir));
}

extern "C" void SSZ_STDCALL PauseBGM(PluginUtil* pu, bool pause)
{
    SSZ_TRACE_CAT(TRACE_SDL, "PauseBGM");
    (void)pu;
    PauseBGM(pause);
}

extern "C" bool SSZ_STDCALL SendOpenBGM(PluginUtil* pu, int32_t channels, int32_t rate)
{
    SSZ_TRACE_CAT(TRACE_SDL, "SendOpenBGM");
    (void)pu;
    return SendOpenBGM(channels, rate);
}

extern "C" void SSZ_STDCALL SendCloseBGM(PluginUtil* pu)
{
    SSZ_TRACE_CAT(TRACE_SDL, "SendCloseBGM");
    (void)pu;
    SendCloseBGM();
}

extern "C" intptr_t SSZ_STDCALL SendWriteBGM(PluginUtil* pu, Reference fn)
{
    SSZ_TRACE_CAT(TRACE_SDL, "SendWriteBGM");
    (void)pu;
    (void)fn;  // fn was historically unused in the old implementation too;
               // preserved as a parameter for ABI compatibility only.
    return SendWriteBGM();
}

extern "C" void SSZ_STDCALL SetVolume(PluginUtil* pu, float bv, float wv, float gv)
{
    SSZ_TRACE_CAT(TRACE_SDL, "SetVolume");
    (void)pu;
    SetVolume(bv, wv, gv);
}

extern "C" void SSZ_STDCALL FadeInBGM(PluginUtil* pu, int time)
{
    SSZ_TRACE_CAT(TRACE_SDL, "FadeInBGM");
    (void)pu;
    FadeInBGM(time);
}

extern "C" void SSZ_STDCALL FadeOutBGM(PluginUtil* pu, int time)
{
    SSZ_TRACE_CAT(TRACE_SDL, "FadeOutBGM");
    (void)pu;
    FadeOutBGM(time);
}

extern "C" bool SSZ_STDCALL RenderMugenZoom(PluginUtil* pu, Reference* pluginbuf, int32_t rle,
    float rcy, float rcx, SDL_Rect* pdstr, int32_t alpha,
    uint32_t roto, float rasterxadd, float yscl, float xbotscl, float xtopscl,
    SDL_Rect* tile, float ty, float cx, SDL_Rect* psrcr,
    uint16_t ckey, uint32_t* ppal, Reference img)
{
    SSZ_TRACE_CAT(TRACE_SDL, "RenderMugenZoom");
    (void)pu;
    return RenderMugenZoom(pluginbuf, rle, rcy, rcx, pdstr, alpha,
        roto, rasterxadd, yscl, xbotscl, xtopscl,
        tile, ty, cx, psrcr, ckey, ppal, img);
}

extern "C" bool SSZ_STDCALL RenderFontBatch(PluginUtil* pu, int32_t count,
    int32_t* glyphData,
    float spacing,
    float yscl,
    float xscl,
    SDL_Rect* window,
    int32_t alpha,
    int32_t glyphH,
    int32_t atlasStride,
    uint32_t* ppal,
    float baseY,
    float baseX,
    uint8_t* atlasPixels)
{
    SSZ_TRACE_CAT(TRACE_SDL, "RenderFontBatch");
    (void)pu;
    return RenderFontBatch(count, glyphData, spacing, yscl, xscl, window,
        alpha, glyphH, atlasStride, ppal, baseY, baseX, atlasPixels);
}

extern "C" bool SSZ_STDCALL RenderMugenShadow(PluginUtil* pu, Reference* pluginbuf, int32_t rle,
    float rcy, float rcx, SDL_Rect* pdstr, int32_t alpha,
    uint32_t roto, float vscl, float yscl, float xscl,
    float ty, float cx, SDL_Rect* psrcr, uint32_t color, Reference img)
{
    SSZ_TRACE_CAT(TRACE_SDL, "RenderMugenShadow");
    return RenderMugenShadow(pluginbuf, rle, rcy, rcx, pdstr, alpha,
        roto, vscl, yscl, xscl, ty, cx, psrcr, color, img);
}

extern "C" uint32_t SSZ_STDCALL Load8bitTexture(PluginUtil* pu, int32_t h, int32_t w, uint8_t* ppxl)
{
    SSZ_TRACE_CAT(TRACE_SDL, "Load8bitTexture");
    (void)pu;
    return Load8bitTexture(h, w, ppxl);
}

extern "C" uint32_t SSZ_STDCALL LoadPngTexture(PluginUtil* pu, FILE* fp, int32_t* h, int32_t* w)
{
    SSZ_TRACE_CAT(TRACE_SDL, "LoadPngTexture");
    (void)pu;
    return LoadPngTexture(fp, h, w);
}

extern "C" void SSZ_STDCALL DeleteGlTexture(PluginUtil* pu, uint32_t texid)
{
    SSZ_TRACE_CAT(TRACE_SDL, "DeleteGlTexture");
    (void)pu;
    DeleteGlTexture(texid);
}

extern "C" void SSZ_STDCALL GlSwapBuffers(PluginUtil* pu)
{
    SSZ_TRACE_CAT(TRACE_SDL, "GlSwapBuffers");
    (void)pu;
    GlSwapBuffers();
}

extern "C" bool SSZ_STDCALL InitMugenGl(PluginUtil* pu)
{
    SSZ_TRACE_CAT(TRACE_SDL, "InitMugenGl");
    (void)pu;
    return InitMugenGl();
}

extern "C" bool SSZ_STDCALL RenderMugenGl(PluginUtil* pu, float rcy, float rcx, SDL_Rect* dstr, int alpha,
    float angle, float rasterxadd, float vscl, float yscl,
    float xbotscl, float xtopscl, SDL_Rect* tile, float y, float x,
    SDL_Rect* rect, int mask, uint8_t* ppal, uint32_t texid)
{
    SSZ_TRACE_CAT(TRACE_SDL, "RenderMugenGl");
    (void)pu;
    return RenderMugenGl(rcy, rcx, dstr, alpha, angle, rasterxadd, vscl, yscl,
        xbotscl, xtopscl, tile, y, x, rect, mask, ppal, texid);
}

extern "C" bool SSZ_STDCALL RenderMugenGlFc(PluginUtil* pu, float mulb, float mulg, float mulr,
    float addb, float addg, float addr, float color, bool neg,
    float rcy, float rcx, SDL_Rect* dstr, int alpha,
    float angle, float rasterxadd, float vscl, float yscl,
    float xbotscl, float xtopscl, SDL_Rect* tile, float y, float x,
    SDL_Rect* rect, uint32_t texid)
{
    SSZ_TRACE_CAT(TRACE_SDL, "RenderMugenGlFc");
    (void)pu;
    return RenderMugenGlFc(mulb, mulg, mulr, addb, addg, addr, color, neg,
        rcy, rcx, dstr, alpha, angle, rasterxadd, vscl, yscl,
        xbotscl, xtopscl, tile, y, x, rect, texid);
}

extern "C" bool SSZ_STDCALL RenderMugenGlFcS(PluginUtil* pu, uint32_t color,
    float rcy, float rcx, SDL_Rect* dstr, int alpha,
    float angle, float rasterxadd, float vscl, float yscl,
    float xbotscl, float xtopscl, SDL_Rect* tile, float y, float x,
    SDL_Rect* rect, uint32_t texid)
{
    SSZ_TRACE_CAT(TRACE_SDL, "RenderMugenGlFcS");
    (void)pu;
    return RenderMugenGlFcS(color, rcy, rcx, dstr, alpha, angle, rasterxadd,
        vscl, yscl, xbotscl, xtopscl, tile, y, x, rect, texid);
}

extern "C" void SSZ_STDCALL MugenFillGl(PluginUtil* pu, int32_t alpha, uint32_t color, SDL_Rect rect)
{
    SSZ_TRACE_CAT(TRACE_SDL, "MugenFillGl");
    (void)pu;
    MugenFillGl(alpha, color, rect);
}

extern "C" bool SSZ_STDCALL BindGlContext(PluginUtil* pu)
{
    SSZ_TRACE_CAT(TRACE_SDL, "BindGlContext");
    (void)pu;
    return BindGlContext();
}

extern "C" bool SSZ_STDCALL UnbindGlContext(PluginUtil* pu)
{
    SSZ_TRACE_CAT(TRACE_SDL, "UnbindGlContext");
    (void)pu;
    return UnbindGlContext();
}

// =========================================================================
// Action wrappers — old ABI -> native C++
// =========================================================================

#if IKEMEN_NATIVE_ACTION_LIB
#include "ssz_native/action_service.hpp"

extern "C" void SSZ_STDCALL ActionInit(PluginUtil* pu)
{
    (void)pu;
    ikemen::ssz_native::action_init();
}

#else
// When IKEMEN_NATIVE_ACTION_LIB=0, the bridge wrappers don't exist
// and the action_static.hpp stubs provide no-op registration. The
// SSZ action.ssz script is used instead.
#endif

// =========================================================================
// BG wrappers — old ABI -> native C++
// =========================================================================

#if IKEMEN_NATIVE_BG_LIB
#include "ssz_native/bg_service.hpp"

extern "C" void SSZ_STDCALL BgInit(PluginUtil* pu)
{
    (void)pu;
    ikemen::ssz_native::bg_init();
}

extern "C" void SSZ_STDCALL BgSplitParams(PluginUtil* pu, Reference paramStr, Reference* out)
{
    (void)pu;
    (void)paramStr;
    (void)out;
    // Split params deferred — needs Reference string conversion and array creation
}

#else
// When IKEMEN_NATIVE_BG_LIB=0, the bridge wrappers don't exist
// and the bg_static.hpp stubs provide no-op registration. The
// SSZ bg.ssz script is used instead.
#endif

// =========================================================================
// Debug Script wrappers — old ABI -> native C++
// =========================================================================

#if IKEMEN_NATIVE_DEBUG_SCRIPT_LIB
#include "ssz_native/debug_script_service.hpp"

extern "C" void SSZ_STDCALL DebugLoadFile(PluginUtil* pu, Reference file, Reference* out)
{
    pu->setSSZFunc();
    (void)file;
    std::string result = ikemen::ssz_native::debug_load_file();
    out->releaseanddelete();
    if (result.empty()) return;
    pu->astrToRef(CP_UTF8, *out, result);
}

extern "C" void SSZ_STDCALL DebugRunFile(PluginUtil* pu, Reference file, Reference* out)
{
    pu->setSSZFunc();
    (void)file;
    std::string result = ikemen::ssz_native::debug_run_file();
    out->releaseanddelete();
    if (result.empty()) return;
    pu->astrToRef(CP_UTF8, *out, result);
}

#else
// When IKEMEN_NATIVE_DEBUG_SCRIPT_LIB=0, the bridge wrappers don't exist and the
// debug_script_static.hpp stubs provide no-op registration. The SSZ debug-script.ssz
// script is used instead.
#endif

// =========================================================================
// Font wrappers — old ABI -> native C++
// =========================================================================

#if IKEMEN_NATIVE_FONT_LIB
#include "ssz_native/font_service.hpp"

extern "C" void SSZ_STDCALL FontInit(PluginUtil* pu)
{
    (void)pu;
    ikemen::ssz_native::font_init();
}

extern "C" void SSZ_STDCALL FontRenderText(PluginUtil* pu)
{
    (void)pu;
    ikemen::ssz_native::font_render_text();
}

#else
// When IKEMEN_NATIVE_FONT_LIB=0, the bridge wrappers don't exist
// and the font_static.hpp stubs provide no-op registration. The
// SSZ font.ssz script is used instead.
#endif

// =========================================================================
// Char wrappers — old ABI -> native C++
// =========================================================================

#if IKEMEN_NATIVE_CHAR_LIB
#include "ssz_native/char_service.hpp"

extern "C" void SSZ_STDCALL CharInit(PluginUtil* pu)
{
    (void)pu;
    ikemen::ssz_native::char_init();
}

#else
// When IKEMEN_NATIVE_CHAR_LIB=0, the bridge wrappers don't exist and the
// char_static.hpp stubs provide no-op registration. The SSZ char.ssz
// script is used instead.
#endif

// =========================================================================
// Command wrappers — old ABI -> native C++
// =========================================================================

#if IKEMEN_NATIVE_COMMAND_LIB
#include "ssz_native/command_service.hpp"

extern "C" void SSZ_STDCALL CommandInit(PluginUtil* pu)
{
    (void)pu;
    ikemen::ssz_native::command_init();
}

#else
// When IKEMEN_NATIVE_COMMAND_LIB=0, the bridge wrappers don't exist
// and the command_static.hpp stubs provide no-op registration. The
// SSZ command.ssz script is used instead.
#endif

// =========================================================================
// Fight wrappers — old ABI -> native C++
// =========================================================================

#if IKEMEN_NATIVE_FIGHT_LIB
#include "ssz_native/fight_service.hpp"

extern "C" void SSZ_STDCALL FightInit(PluginUtil* pu)
{
    (void)pu;
    ikemen::ssz_native::fight_init();
}

#else
// When IKEMEN_NATIVE_FIGHT_LIB=0, the bridge wrappers don't exist
// and the fight_static.hpp stubs provide no-op registration. The
// SSZ fight.ssz script is used instead.
#endif

// =========================================================================
// Fighting wrappers — old ABI -> native C++
// =========================================================================

#if IKEMEN_NATIVE_FIGHTING_LIB
#include "ssz_native/fighting_service.hpp"

extern "C" void SSZ_STDCALL FightingInit(PluginUtil* pu)
{
    (void)pu;
    ikemen::ssz_native::fighting_init();
}

#else
// When IKEMEN_NATIVE_FIGHTING_LIB=0, the bridge wrappers don't exist
// and the fighting_static.hpp stubs provide no-op registration. The
// SSZ fighting.ssz script is used instead.
#endif

// =========================================================================
// System Script wrappers — old ABI -> native C++
// =========================================================================

#if IKEMEN_NATIVE_SYSTEM_SCRIPT_LIB
#include "ssz_native/system_script_service.hpp"

extern "C" void SSZ_STDCALL SystemScriptInit(PluginUtil* pu)
{
    (void)pu;
    ikemen::ssz_native::system_script_init();
}

#else
// When IKEMEN_NATIVE_SYSTEM_SCRIPT_LIB=0, the bridge wrappers don't exist
// and the system_script_static.hpp stubs provide no-op registration. The
// SSZ system-script.ssz script is used instead.
#endif

// =========================================================================
// Trigger Script wrappers — old ABI -> native C++
// =========================================================================

#if IKEMEN_NATIVE_TRIGGER_SCRIPT_LIB
#include "ssz_native/trigger_script_service.hpp"

extern "C" void SSZ_STDCALL TriggerScriptRegisterFunction(PluginUtil* pu)
{
    (void)pu;
    ikemen::ssz_native::register_function();
}

#else
// When IKEMEN_NATIVE_TRIGGER_SCRIPT_LIB=0, the bridge wrappers don't exist
// and the trigger_script_static.hpp stubs provide no-op registration. The
// SSZ trigger-script.ssz script is used instead.
#endif

// =========================================================================
// Stage wrappers — old ABI -> native C++
// =========================================================================

#if IKEMEN_NATIVE_STAGE_LIB
#include "ssz_native/stage_service.hpp"

extern "C" void SSZ_STDCALL StageInit(PluginUtil* pu)
{
    (void)pu;
    ikemen::ssz_native::stage_init();
}

extern "C" void SSZ_STDCALL StageLoad(PluginUtil* pu, Reference def, Reference* out)
{
    pu->setSSZFunc();
    // Convert SSZ Reference to native path string
    std::wstring wdef = ikemen::ssz_bridge::refToWstring(pu, def);
    std::string defStr(wdef.begin(), wdef.end());
    std::string err = ikemen::ssz_native::stage_load(defStr);
    if (err.empty()) return;
    pu->astrToRef(CP_UTF8, *out, err);
}

extern "C" void SSZ_STDCALL StageAction(PluginUtil* pu)
{
    (void)pu;
    ikemen::ssz_native::stage_action();
}

extern "C" void SSZ_STDCALL StageBgDraw(PluginUtil* pu, int32_t t, float x, float y, float scl)
{
    (void)pu;
    ikemen::ssz_native::stage_bg_draw(t != 0, x, y, scl);
}

extern "C" void SSZ_STDCALL StageClear(PluginUtil* pu)
{
    (void)pu;
    ikemen::ssz_native::stage_clear();
}

extern "C" void SSZ_STDCALL StageReset(PluginUtil* pu)
{
    (void)pu;
    ikemen::ssz_native::stage_reset();
}

#else
// When IKEMEN_NATIVE_STAGE_LIB=0, the bridge wrappers don't exist
// and the stage_static.hpp stubs provide no-op registration. The
// SSZ stage.ssz script is used instead.
#endif

// =========================================================================
// Script wrappers — old ABI -> native C++
// =========================================================================

#if IKEMEN_NATIVE_SCRIPT_LIB
#include "ssz_native/script_service.hpp"

extern "C" void SSZ_STDCALL ScriptInit(PluginUtil* pu)
{
    (void)pu;
    // Get the main Lua state captured during engine initialization.
    // This allows native script_init() to register all 190+ Lua-callable
    // functions (sffNew, sndPlay, key query functions, game state getters/setters)
    // with the Lua state so they are callable from Lua scripts (main.lua, etc.).
    lua_State* L = get_main_lua_state();
    ikemen::ssz_native::script_init(L);
}

#else
// When IKEMEN_NATIVE_SCRIPT_LIB=0, the bridge wrappers don't exist and the
// script_static.hpp stubs provide no-op registration. The SSZ script.ssz
// script is used instead.
#endif

// =========================================================================
// StateBuilder wrappers — old ABI -> native C++
// =========================================================================

#if IKEMEN_NATIVE_STATEBUILDER_LIB
#include "ssz_native/statebuilder_service.hpp"

extern "C" void SSZ_STDCALL StateBuilderInit(PluginUtil* pu)
{
    (void)pu;
    ikemen::ssz_native::statebuilder_init();
}

#else
// When IKEMEN_NATIVE_STATEBUILDER_LIB=0, the bridge wrappers don't exist
// and the statebuilder_static.hpp stubs provide no-op registration. The
// SSZ statebuilder.ssz script is used instead.
#endif

// =========================================================================
// System wrappers — old ABI -> native C++
// =========================================================================

#if IKEMEN_NATIVE_SYSTEM_LIB
#include "ssz_native/system_service.hpp"

extern "C" bool SSZ_STDCALL SystemAddChar(PluginUtil* pu, Reference def)
{
    pu->setSSZFunc();
    std::wstring wdef = ikemen::ssz_bridge::refToWstring(pu, def);
    std::string defStr(wdef.begin(), wdef.end());
    return ikemen::ssz_native::system_add_char(defStr);
}

extern "C" void SSZ_STDCALL SystemAddStage(PluginUtil* pu, Reference def, Reference* out)
{
    pu->setSSZFunc();
    std::wstring wdef = ikemen::ssz_bridge::refToWstring(pu, def);
    std::string defStr(wdef.begin(), wdef.end());
    std::string name = ikemen::ssz_native::system_add_stage(defStr);
    if (!name.empty())
        pu->astrToRef(CP_UTF8, *out, name);
}

extern "C" void SSZ_STDCALL SystemGetStageName(PluginUtil* pu, int32_t i, Reference* out)
{
    pu->setSSZFunc();
    std::string name = ikemen::ssz_native::system_get_stage_name(i);
    if (name.empty()) return;
    pu->astrToRef(CP_UTF8, *out, name);
}

extern "C" int SSZ_STDCALL SystemSetStageNo(PluginUtil* pu, int32_t i)
{
    (void)pu;
    return ikemen::ssz_native::system_set_stage_no(i);
}

extern "C" void SSZ_STDCALL SystemSelectStage(PluginUtil* pu, int32_t no)
{
    (void)pu;
    ikemen::ssz_native::system_select_stage(no);
}

extern "C" bool SSZ_STDCALL SystemAddSelchr(PluginUtil* pu, int32_t pn, int32_t cn, int32_t pl)
{
    (void)pu;
    return ikemen::ssz_native::system_add_selchr(pn, cn, pl);
}

extern "C" void SSZ_STDCALL SystemSelReset(PluginUtil* pu)
{
    (void)pu;
    ikemen::ssz_native::system_sel_reset();
}

#else
// When IKEMEN_NATIVE_SYSTEM_LIB=0, the bridge wrappers don't exist and the
// system_static.hpp stubs provide no-op registration. The SSZ system.ssz
// script is used instead.
#endif

// =========================================================================
// Loader wrappers — old ABI -> native C++
// =========================================================================

#if IKEMEN_NATIVE_LOADER_LIB
#include "ssz_native/loader_service.hpp"

extern "C" void SSZ_STDCALL LoaderError(PluginUtil* pu, Reference msg)
{
    ikemen::ssz_native::loader_error(
        ikemen::ssz_bridge::refToNarrowUtf8(pu, msg));
}

extern "C" bool SSZ_STDCALL LoaderStage(PluginUtil* pu)
{
    (void)pu;
    return ikemen::ssz_native::loader_stage();
}

extern "C" int SSZ_STDCALL LoaderChara(PluginUtil* pu, int32_t pn)
{
    (void)pu;
    return ikemen::ssz_native::loader_chara(pn);
}

extern "C" bool SSZ_STDCALL LoaderStateCompile(PluginUtil* pu)
{
    (void)pu;
    return ikemen::ssz_native::loader_state_compile();
}

extern "C" void SSZ_STDCALL LoaderLoad(PluginUtil* pu)
{
    (void)pu;
    ikemen::ssz_native::loader_load();
}

extern "C" void SSZ_STDCALL LoaderReset(PluginUtil* pu)
{
    (void)pu;
    ikemen::ssz_native::loader_reset();
}

extern "C" bool SSZ_STDCALL LoaderRunTread(PluginUtil* pu)
{
    (void)pu;
    return ikemen::ssz_native::loader_run_tread();
}

#else
// When IKEMEN_NATIVE_LOADER_LIB=0, the bridge wrappers don't exist and the
// loader_static.hpp stubs provide no-op registration. The SSZ loader.ssz
// script is used instead.
#endif

// =========================================================================
// Common wrappers — old ABI -> native C++
// =========================================================================

#if IKEMEN_NATIVE_COMMON_LIB
#include "ssz_native/common_service.hpp"

extern "C" void SSZ_STDCALL CommonFlagInit(PluginUtil* pu)
{
    (void)pu;
    ikemen::ssz_native::common_flag_init();
}

extern "C" void SSZ_STDCALL CommonResetRemapInput(PluginUtil* pu)
{
    (void)pu;
    ikemen::ssz_native::common_reset_remap_input();
}

extern "C" void SSZ_STDCALL CommonSetSize(PluginUtil* pu, int32_t w, int32_t h)
{
    (void)pu;
    ikemen::ssz_native::common_set_size(w, h);
}

extern "C" bool SSZ_STDCALL CommonTickFrame(PluginUtil* pu)
{
    (void)pu;
    return ikemen::ssz_native::common_tick_frame();
}

extern "C" bool SSZ_STDCALL CommonTickNextFrame(PluginUtil* pu)
{
    (void)pu;
    return ikemen::ssz_native::common_tick_next_frame();
}

extern "C" float SSZ_STDCALL CommonTickInterpola(PluginUtil* pu)
{
    (void)pu;
    return ikemen::ssz_native::common_tick_interpola();
}

extern "C" bool SSZ_STDCALL CommonAddFrameTime(PluginUtil* pu, float t)
{
    (void)pu;
    return ikemen::ssz_native::common_add_frame_time(t);
}

extern "C" void SSZ_STDCALL CommonResetFrameTime(PluginUtil* pu)
{
    (void)pu;
    ikemen::ssz_native::common_reset_frame_time();
}

extern "C" bool SSZ_STDCALL CommonMatchOver(PluginUtil* pu)
{
    (void)pu;
    return ikemen::ssz_native::common_match_over();
}

extern "C" int SSZ_STDCALL CommonAtoi(PluginUtil* pu, Reference str)
{
    return ikemen::ssz_native::common_atoi(
        ikemen::ssz_bridge::refToNarrowUtf8(pu, str));
}

extern "C" double SSZ_STDCALL CommonAtof(PluginUtil* pu, Reference str)
{
    return ikemen::ssz_native::common_atof(
        ikemen::ssz_bridge::refToNarrowUtf8(pu, str));
}

extern "C" void SSZ_STDCALL CommonLoadText(PluginUtil* pu, Reference* out, Reference filename, bool unicode)
{
    pu->setSSZFunc();
    std::string result = ikemen::ssz_native::common_load_text(
        ikemen::ssz_bridge::refToNarrowUtf8(pu, filename), unicode);
    if (result.empty()) return;
    pu->astrToRef(CP_UTF8, *out, result);
}

#else
// When IKEMEN_NATIVE_COMMON_LIB=0, the bridge wrappers don't exist and the
// common_static.hpp stubs provide no-op registration. The SSZ common.ssz
// script is used instead.
#endif

// =========================================================================
// SFF wrappers — old ABI -> native C++
// =========================================================================

#if IKEMEN_NATIVE_SFF_LIB
#include "ssz_native/sff_service.hpp"

extern "C" void SSZ_STDCALL SffInit(PluginUtil* pu)
{
    (void)pu;
    ikemen::ssz_native::sff_init();
}

extern "C" void SSZ_STDCALL SffLoadFile(PluginUtil* pu, Reference filename, int32_t chr, Reference* out)
{
    pu->setSSZFunc();
    std::wstring wfn = ikemen::ssz_bridge::refToWstring(pu, filename);
    std::string fn(wfn.begin(), wfn.end());
    std::string err = ikemen::ssz_native::SffData{}.loadFile(fn, chr != 0);
    if (err.empty()) return;
    pu->astrToRef(CP_UTF8, *out, err);
}

extern "C" void SSZ_STDCALL SffGetSprite(PluginUtil* pu, int32_t group, int32_t number, Reference* out)
{
    (void)pu;
    (void)group;
    (void)number;
    (void)out;
    // Sprite lookup deferred — needs Sff instance management
}

#else
// When IKEMEN_NATIVE_SFF_LIB=0, the bridge wrappers don't exist
// and the sff_static.hpp stubs provide no-op registration. The
// SSZ sff.ssz script is used instead.
#endif

// =========================================================================
// Share wrappers — old ABI -> native C++
// =========================================================================

#if IKEMEN_NATIVE_SHARE_LIB
#include "ssz_native/share_service.hpp"

extern "C" void SSZ_STDCALL ShareCopy(PluginUtil* pu)
{
    (void)pu;
    ikemen::ssz_native::share_copy();
}

extern "C" void SSZ_STDCALL SharePush(PluginUtil* pu)
{
    (void)pu;
    ikemen::ssz_native::share_push();
}

#else
// When IKEMEN_NATIVE_SHARE_LIB=0, the bridge wrappers don't exist and the
// share_static.hpp stubs provide no-op registration. The SSZ share.ssz
// script is used instead.
#endif

// =========================================================================
// Sound Resource wrappers — old ABI -> native C++
// =========================================================================

#if IKEMEN_NATIVE_SOUND_RES_LIB
#include "ssz_native/sound_resource_service.hpp"

extern "C" void SSZ_STDCALL SoundResourceInit(PluginUtil* pu)
{
    (void)pu;
    ikemen::ssz_native::sound_resource_init();
}

extern "C" void SSZ_STDCALL SoundSndbufClear(PluginUtil* pu)
{
    (void)pu;
    ikemen::ssz_native::sndbuf_clear();
}

extern "C" void SSZ_STDCALL SoundMixSounds(PluginUtil* pu)
{
    (void)pu;
    ikemen::ssz_native::mix_sounds();
}

extern "C" void SSZ_STDCALL SoundPlaySound(PluginUtil* pu)
{
    (void)pu;
    ikemen::ssz_native::play_sound();
}

extern "C" void SSZ_STDCALL SoundStopSound(PluginUtil* pu)
{
    (void)pu;
    ikemen::ssz_native::stop_sound();
}

#else
// When IKEMEN_NATIVE_SOUND_RES_LIB=0, the bridge wrappers don't exist
// and the sound_resource_static.hpp stubs provide no-op registration. The
// SSZ sound.ssz script is used instead.
#endif

// =========================================================================
// Video wrappers — old ABI -> native C++
// =========================================================================

#if IKEMEN_NATIVE_VIDEO_LIB
#include "ssz_native/video_service.hpp"

extern "C" void SSZ_STDCALL VideoPlay(PluginUtil* pu)
{
    (void)pu;
    ikemen::ssz_native::video_play();
}

#else
// When IKEMEN_NATIVE_VIDEO_LIB=0, the bridge wrappers don't exist
// and the video_static.hpp stubs provide no-op registration. The
// SSZ video.ssz script is used instead.
#endif

// =========================================================================
// SDL Event wrappers — old ABI -> native C++
// =========================================================================

#if IKEMEN_NATIVE_SDLEVENT_LIB
#include "ssz_native/sdlevent_service.hpp"

extern "C" bool SSZ_STDCALL SdleventEventUpdate(PluginUtil* pu)
{
    SSZ_TRACE_CAT(TRACE_SDL, "SdleventEventUpdate");
    (void)pu;
    return ikemen::ssz_native::sdlevent_event_update();
}

extern "C" bool SSZ_STDCALL SdleventEvent(PluginUtil* pu, int32_t fps)
{
    SSZ_TRACE_CAT(TRACE_SDL, "SdleventEvent");
    (void)pu;
    return ikemen::ssz_native::sdlevent_event(static_cast<int>(fps));
}

#else
// When IKEMEN_NATIVE_SDLEVENT_LIB=0, the bridge wrappers don't exist
// and the sdlevent_static.hpp stubs provide no-op registration. The
// SSZ sdlevent.ssz script is used instead.
#endif
