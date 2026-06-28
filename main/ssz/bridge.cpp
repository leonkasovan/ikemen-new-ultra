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
// Class forward-declared in main/sound/sound.cpp
// -----------------------------------------------------------------------
class Client;

// -----------------------------------------------------------------------
// Class forward-declared in main/ogg/ogg.cpp
// -----------------------------------------------------------------------
class OggVorbis;

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
// Native implementations defined in main/file/file.cpp
// -----------------------------------------------------------------------
#ifdef _WIN32
#include <regex>
#define RNS std
#else
#include <boost/regex.hpp>
#define RNS boost
#endif

uint32_t SSZ_STDCALL TickCount();
int64_t  SSZ_STDCALL UnixTime();
void     SSZ_STDCALL Alert(const std::wstring& title, const std::wstring& mes);
void     SSZ_STDCALL ThreadDelay(uint32_t ui);
RNS::wregex* SSZ_STDCALL NewRegex(bool i, const std::wstring& ptn, std::wstring* error);
void         SSZ_STDCALL DeleteRegex(RNS::wregex* re);
std::vector<ikemen::ssz_bridge::RegexMatchInfo> SSZ_STDCALL RegexSearch(const std::wstring& str, RNS::wregex* re);
bool     SSZ_STDCALL ShellOpen(bool act, bool wait, const std::wstring& direct, const std::wstring& param, const std::wstring& file);
bool     SSZ_STDCALL MoveTrash(const std::wstring& file);
void     SSZ_STDCALL SocketClose(SOCKET *psoc);
bool     SSZ_STDCALL SocketConnect(bool nodelay, int32_t timeout, const std::string& port, const std::string& host, SOCKET *psoc);
bool     SSZ_STDCALL SocketListen(bool ipv4, int32_t backlog, const std::string& port, SOCKET *psoc);
SOCKET   SSZ_STDCALL SocketAccept(bool nodelay, int32_t timeout, SOCKET soc);
bool     SSZ_STDCALL SocketSend(intptr_t size, const char *p, SOCKET *psoc);
intptr_t SSZ_STDCALL SocketSendAry(intptr_t size, const void *data, intptr_t bytes, SOCKET *psoc);
bool     SSZ_STDCALL SocketRecv(intptr_t size, char *p, SOCKET *psoc);
intptr_t SSZ_STDCALL SocketRecvAry(intptr_t size, void *data, intptr_t bytes, SOCKET *psoc);
Client* SSZ_STDCALL NewClient();
void    SSZ_STDCALL DeleteClient(Client* client);
bool    SSZ_STDCALL ClientStart(Client* client);
bool    SSZ_STDCALL ClientStop(Client* client);
bool    SSZ_STDCALL ClientBufferReady(Client* client);
bool    SSZ_STDCALL ClientSetBuffer(const float* buffer, intptr_t frames, Client* client);
struct lua_State;
bool       SSZ_STDCALL YesNo(const std::wstring& r);
void       SSZ_STDCALL VeryUnsafeCopy(intptr_t size, void *src, void *dst);
std::wstring SSZ_STDCALL GetClipboardStr();
intptr_t   SSZ_STDCALL TazyuuCheck(const std::wstring& name);
void       SSZ_STDCALL CloseTazyuuHandle(intptr_t mutex);
std::wstring SSZ_STDCALL GetInifileString(const std::wstring& def, const std::wstring& key, const std::wstring& app, const std::wstring& file);
int32_t    SSZ_STDCALL GetInifileInt(int32_t def, const std::wstring& key, const std::wstring& app, const std::wstring& file);
bool       SSZ_STDCALL WriteInifileString(const std::wstring& str, const std::wstring& key, const std::wstring& app, const std::wstring& file);
bool       SSZ_STDCALL UnCompress(const void* data, intptr_t bytes, std::vector<uint8_t>& output);
void       SSZ_STDCALL UbytesToStr(const void* data, intptr_t bytes, UINT cp, std::wstring& output);
void       SSZ_STDCALL StrToUbytes(const void* data, intptr_t bytes, UINT cp, std::vector<uint8_t>& output);
void       SSZ_STDCALL AsciiToLocal(const void* data, intptr_t bytes, std::wstring& output);
void       SSZ_STDCALL SetSharedString(const std::wstring& str);
std::wstring SSZ_STDCALL GetSharedString();
std::wstring SSZ_STDCALL InputStr(const std::wstring& title);
void       SSZ_STDCALL LuaInit(intptr_t refcopy, intptr_t refdest, SSZCALLBACK callback, void* handle);
lua_State* SSZ_STDCALL NewState();
void       SSZ_STDCALL Close(lua_State* L);
bool       SSZ_STDCALL RunFile(const std::string& filename, lua_State* L);
bool       SSZ_STDCALL RunString(const std::string& s, lua_State* L);
int32_t    SSZ_STDCALL GetTop(lua_State* L);
void       SSZ_STDCALL GetGlobal(const std::string& var, lua_State* L);
void       SSZ_STDCALL Register(intptr_t func, const std::string& var, lua_State* L);
bool       SSZ_STDCALL Pcall(int32_t nresults, int32_t nargs, lua_State* L);
void       SSZ_STDCALL Pop(int32_t n, lua_State* L);
void       SSZ_STDCALL PushNumber(double n, lua_State* L);
bool       SSZ_STDCALL IsNumber(int32_t idx, lua_State* L);
double     SSZ_STDCALL ToNumber(int32_t idx, lua_State* L);
void       SSZ_STDCALL PushBoolean(bool b, lua_State* L);
bool       SSZ_STDCALL IsBoolean(int32_t idx, lua_State* L);
bool       SSZ_STDCALL ToBoolean(int32_t idx, lua_State* L);
void       SSZ_STDCALL PushString(const std::string& s, lua_State* L);
bool       SSZ_STDCALL IsString(int32_t idx, lua_State* L);
void       SSZ_STDCALL ToString(int32_t idx, lua_State* L, std::string& output);
void       SSZ_STDCALL PushRef(DynamicRef* userdata, lua_State* L);
void       SSZ_STDCALL ToRef(int32_t idx, DynamicRef* userdata, lua_State* L);
OggVorbis* SSZ_STDCALL NewOggVorbis();
void       SSZ_STDCALL DeleteOggVorbis(OggVorbis* ov);
bool       SSZ_STDCALL OggVorbisOpen(const std::wstring& file, OggVorbis* ov);
void       SSZ_STDCALL OggVorbisClear(OggVorbis* ov);
int64_t    SSZ_STDCALL OggVorbisPcmTotal(OggVorbis* ov);
int32_t    SSZ_STDCALL OggVorbisChannels(OggVorbis* ov);
int32_t    SSZ_STDCALL OggVorbisRate(OggVorbis* ov);
intptr_t   SSZ_STDCALL OggVorbisRead(int16_t* buffer, intptr_t length, OggVorbis* ov);
int32_t    SSZ_STDCALL OggVorbisSeek(double time, OggVorbis* ov);
double SSZ_STDCALL Sin(double x);
double SSZ_STDCALL Cos(double x);
double SSZ_STDCALL Tan(double x);
double SSZ_STDCALL ASin(double x);
double SSZ_STDCALL ACos(double x);
double SSZ_STDCALL ATan(double x);
double SSZ_STDCALL Log(double y, double x);
double SSZ_STDCALL Ln(double x);
double SSZ_STDCALL Exp(double x);
double SSZ_STDCALL Sqrt(double x);
double SSZ_STDCALL Ceil(double x);
double SSZ_STDCALL Floor(double x);
bool   SSZ_STDCALL IsFinite(double x);
bool   SSZ_STDCALL IsInf(double x);
bool   SSZ_STDCALL IsNaN(double x);
intptr_t SSZ_STDCALL Open(const std::wstring& md, const std::wstring& fn);
void     SSZ_STDCALL FileClose(FILE *pFile);
bool     SSZ_STDCALL Read(intptr_t size, void *p, FILE *pFile);
intptr_t SSZ_STDCALL ReadAry(intptr_t size, void *data, intptr_t bytes, FILE *pFile);
bool     SSZ_STDCALL Write(intptr_t size, const void *p, FILE *pFile);
intptr_t SSZ_STDCALL WriteAry(intptr_t size, const void *data, intptr_t bytes, FILE *pFile);
bool     SSZ_STDCALL Seek(int32_t origin, int64_t offset, FILE *pFile);
std::wstring SSZ_STDCALL LoadAsciiText(const std::wstring& path);
bool     SSZ_STDCALL SaveAsciiText(const std::wstring& txt, const std::wstring& path);
bool     SSZ_STDCALL Delete(const std::wstring& file);
bool     SSZ_STDCALL Move(const std::wstring& newn, const std::wstring& oldn);
bool     SSZ_STDCALL Copy(bool overwrite, const std::wstring& dist, const std::wstring& source);
std::vector<std::wstring> SSZ_STDCALL Find(const std::wstring& pattern);
std::vector<std::wstring> SSZ_STDCALL FindDir(const std::wstring& pattern);
bool     SSZ_STDCALL CreateDir(const std::wstring& dir);
bool     SSZ_STDCALL RemoveDir(const std::wstring& dir);
bool     SSZ_STDCALL SetCurrentDir(const std::wstring& dir);
std::wstring SSZ_STDCALL GetCurrentDir();

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
    (void)pu;
    return Open(
ikemen::ssz_bridge::refToWstring(pu, md), ikemen::ssz_bridge::refToWstring(pu, fn));
}

extern "C" void SSZ_STDCALL FileClose(PluginUtil* pu, FILE *pFile)
{
    (void)pu;
    FileClose(pFile);
}

extern "C" bool SSZ_STDCALL Read(PluginUtil* pu, intptr_t size, void *p, FILE *pFile)
{
    (void)pu;
    return Read(size, p, pFile);
}

extern "C" intptr_t SSZ_STDCALL ReadAry(PluginUtil* pu, intptr_t size, Reference ary, FILE *pFile)
{
    (void)pu;
    return ReadAry(size, ary.atpos(), ary.len(), pFile);
}

extern "C" bool SSZ_STDCALL Write(PluginUtil* pu, intptr_t size, void *p, FILE *pFile)
{
    (void)pu;
    return Write(size, p, pFile);
}

extern "C" intptr_t SSZ_STDCALL WriteAry(PluginUtil* pu, intptr_t size, Reference ary, FILE *pFile)
{
    (void)pu;
    return WriteAry(size, ary.atpos(), ary.len(), pFile);
}

extern "C" bool SSZ_STDCALL Seek(PluginUtil* pu, int32_t origin, int64_t offset, FILE *pFile)
{
    (void)pu;
    return Seek(origin, offset, pFile);
}

extern "C" void SSZ_STDCALL LoadAsciiText(PluginUtil* pu, Reference *pr, Reference r)
{
    pu->setSSZFunc();
    std::wstring text = LoadAsciiText(
ikemen::ssz_bridge::refToWstring(pu, r));
    pr->releaseanddelete();
    if (text.empty()) return;
    pu->wstrToRef(*pr, text);
}

extern "C" bool SSZ_STDCALL SaveAsciiText(PluginUtil* pu, Reference txt, Reference r)
{
    return SaveAsciiText(
        ikemen::ssz_bridge::refToWstring(pu, txt),
        ikemen::ssz_bridge::refToWstring(pu, r));
}

extern "C" bool SSZ_STDCALL Delete(PluginUtil* pu, Reference file)
{
    return Delete(ikemen::ssz_bridge::refToWstring(pu, file));
}

extern "C" bool SSZ_STDCALL Move(PluginUtil* pu, Reference newn, Reference oldn)
{
    return Move(
        ikemen::ssz_bridge::refToWstring(pu, newn),
        ikemen::ssz_bridge::refToWstring(pu, oldn));
}

extern "C" bool SSZ_STDCALL Copy(PluginUtil* pu, bool overwrite, Reference dist, Reference source)
{
    return Copy(
        overwrite,
        ikemen::ssz_bridge::refToWstring(pu, dist),
        ikemen::ssz_bridge::refToWstring(pu, source));
}

extern "C" void SSZ_STDCALL Find(PluginUtil* pu, Reference *fls, Reference fn)
{
    std::vector<std::wstring> files = Find(ikemen::ssz_bridge::refToWstring(pu, fn));
    ikemen::ssz_bridge::vectorToRefList(pu, fls, files);
}

extern "C" void SSZ_STDCALL FindDir(PluginUtil* pu, Reference *fls, Reference fn)
{
    std::vector<std::wstring> dirs = FindDir(ikemen::ssz_bridge::refToWstring(pu, fn));
    ikemen::ssz_bridge::vectorToRefList(pu, fls, dirs);
}

extern "C" bool SSZ_STDCALL CreateDir(PluginUtil* pu, Reference dir)
{
    return CreateDir(ikemen::ssz_bridge::refToWstring(pu, dir));
}

extern "C" bool SSZ_STDCALL RemoveDir(PluginUtil* pu, Reference dir)
{
    return RemoveDir(ikemen::ssz_bridge::refToWstring(pu, dir));
}

extern "C" bool SSZ_STDCALL SetCurrentDir(PluginUtil* pu, Reference dir)
{
    return SetCurrentDir(ikemen::ssz_bridge::refToWstring(pu, dir));
}

extern "C" void SSZ_STDCALL GetCurrentDir(PluginUtil* pu, Reference* dir)
{
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
    (void)pu;
    SocketClose(psoc);
}

extern "C" bool SSZ_STDCALL SocketConnect(PluginUtil* pu, bool nodelay, int32_t timeout,
    Reference port, Reference host, SOCKET *psoc)
{
    return SocketConnect(
        nodelay, timeout,
        ikemen::ssz_bridge::refToNarrowUtf8(pu, port),
        ikemen::ssz_bridge::refToNarrowUtf8(pu, host),
        psoc);
}

extern "C" bool SSZ_STDCALL SocketListen(PluginUtil* pu, bool ipv4, int32_t backlog,
    Reference port, SOCKET *psoc)
{
    return SocketListen(
        ipv4, backlog,
        ikemen::ssz_bridge::refToNarrowUtf8(pu, port),
        psoc);
}

extern "C" SOCKET SSZ_STDCALL SocketAccept(PluginUtil* pu, bool nodelay, int32_t timeout, SOCKET soc)
{
    (void)pu;
    return SocketAccept(nodelay, timeout, soc);
}

extern "C" bool SSZ_STDCALL SocketSend(PluginUtil* pu, intptr_t size, char *p, SOCKET *psoc)
{
    (void)pu;
    return SocketSend(size, p, psoc);
}

extern "C" intptr_t SSZ_STDCALL SocketSendAry(PluginUtil* pu, intptr_t size, Reference ary, SOCKET *psoc)
{
    (void)pu;
    return SocketSendAry(size, ary.atpos(), ary.len(), psoc);
}

extern "C" bool SSZ_STDCALL SocketRecv(PluginUtil* pu, intptr_t size, char *p, SOCKET *psoc)
{
    (void)pu;
    return SocketRecv(size, p, psoc);
}

extern "C" intptr_t SSZ_STDCALL SocketRecvAry(PluginUtil* pu, intptr_t size, Reference ary, SOCKET *psoc)
{
    (void)pu;
    return SocketRecvAry(size, ary.atpos(), ary.len(), psoc);
}

// =========================================================================
// Lua wrappers — old ABI -> native C++
// =========================================================================

extern "C" void SSZ_STDCALL LuaInit(PluginUtil* pu, intptr_t refcopy, intptr_t refdest)
{
    LuaInit(refcopy, refdest, pu->psf->callback, pu->handle);
}

extern "C" lua_State* SSZ_STDCALL NewState(PluginUtil* pu)
{
    (void)pu;
    return NewState();
}

extern "C" void SSZ_STDCALL Close(PluginUtil* pu, lua_State* L)
{
    (void)pu;
    Close(L);
}

extern "C" bool SSZ_STDCALL RunFile(PluginUtil* pu, Reference filename, lua_State* L)
{
    return RunFile(ikemen::ssz_bridge::refToNarrowUtf8(pu, filename), L);
}

extern "C" bool SSZ_STDCALL RunString(PluginUtil* pu, Reference s, lua_State* L)
{
    return RunString(ikemen::ssz_bridge::refToNarrowUtf8(pu, s), L);
}

extern "C" int32_t SSZ_STDCALL GetTop(PluginUtil* pu, lua_State* L)
{
    (void)pu;
    return GetTop(L);
}

extern "C" void SSZ_STDCALL GetGlobal(PluginUtil* pu, Reference var, lua_State* L)
{
    GetGlobal(ikemen::ssz_bridge::refToNarrowUtf8(pu, var), L);
}

extern "C" void SSZ_STDCALL Register(PluginUtil* pu, intptr_t func, Reference var, lua_State* L)
{
    Register(func, ikemen::ssz_bridge::refToNarrowUtf8(pu, var), L);
}

extern "C" bool SSZ_STDCALL Pcall(PluginUtil* pu, int32_t nresults, int32_t nargs, lua_State* L)
{
    (void)pu;
    return Pcall(nresults, nargs, L);
}

extern "C" void SSZ_STDCALL Pop(PluginUtil* pu, int32_t n, lua_State* L)
{
    (void)pu;
    Pop(n, L);
}

extern "C" void SSZ_STDCALL PushNumber(PluginUtil* pu, double n, lua_State* L)
{
    (void)pu;
    PushNumber(n, L);
}

extern "C" bool SSZ_STDCALL IsNumber(PluginUtil* pu, int32_t idx, lua_State* L)
{
    (void)pu;
    return IsNumber(idx, L);
}

extern "C" double SSZ_STDCALL ToNumber(PluginUtil* pu, int32_t idx, lua_State* L)
{
    (void)pu;
    return ToNumber(idx, L);
}

extern "C" void SSZ_STDCALL PushBoolean(PluginUtil* pu, bool b, lua_State* L)
{
    (void)pu;
    PushBoolean(b, L);
}

extern "C" bool SSZ_STDCALL IsBoolean(PluginUtil* pu, int32_t idx, lua_State* L)
{
    (void)pu;
    return IsBoolean(idx, L);
}

extern "C" bool SSZ_STDCALL ToBoolean(PluginUtil* pu, int32_t idx, lua_State* L)
{
    (void)pu;
    return ToBoolean(idx, L);
}

extern "C" void SSZ_STDCALL PushString(PluginUtil* pu, Reference s, lua_State* L)
{
    PushString(ikemen::ssz_bridge::refToNarrowUtf8(pu, s), L);
}

extern "C" bool SSZ_STDCALL IsString(PluginUtil* pu, int32_t idx, lua_State* L)
{
    (void)pu;
    return IsString(idx, L);
}

extern "C" void SSZ_STDCALL ToString(PluginUtil* pu, int32_t idx, Reference* s, lua_State* L)
{
    pu->setSSZFunc();
    std::string output;
    ToString(idx, L, output);
    if (output.empty()) return;
    pu->astrToRef(CP_UTF8, *s, output);
}

extern "C" void SSZ_STDCALL PushRef(PluginUtil* pu, DynamicRef* userdata, lua_State* L)
{
    (void)pu;
    PushRef(userdata, L);
}

extern "C" void SSZ_STDCALL ToRef(PluginUtil* pu, int32_t idx, DynamicRef* userdata, lua_State* L)
{
    (void)pu;
    ToRef(idx, userdata, L);
}

// =========================================================================
// Mesdialog wrappers — old ABI -> native C++
// =========================================================================

extern "C" bool SSZ_STDCALL YesNo(PluginUtil* pu, Reference r)
{
    return YesNo(ikemen::ssz_bridge::refToWstring(pu, r));
}

extern "C" void SSZ_STDCALL VeryUnsafeCopy(PluginUtil* pu, intptr_t size, void *src, void *dst)
{
    (void)pu;
    VeryUnsafeCopy(size, src, dst);
}

extern "C" bool SSZ_STDCALL GetClipboardStr(PluginUtil* pu, Reference *r)
{
    pu->setSSZFunc();
    std::wstring result = GetClipboardStr();
    if (result.empty()) return false;
    pu->wstrToRef(*r, result);
    return true;
}

extern "C" intptr_t SSZ_STDCALL TazyuuCheck(PluginUtil* pu, Reference name)
{
    return TazyuuCheck(ikemen::ssz_bridge::refToWstring(pu, name));
}

extern "C" void SSZ_STDCALL CloseTazyuuHandle(PluginUtil* pu, intptr_t mutex)
{
    (void)pu;
    CloseTazyuuHandle(mutex);
}

extern "C" void SSZ_STDCALL GetInifileString(PluginUtil* pu, Reference* pstr,
    Reference def, Reference key, Reference app, Reference file)
{
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
    return GetInifileInt(
        def,
        ikemen::ssz_bridge::refToWstring(pu, key),
        ikemen::ssz_bridge::refToWstring(pu, app),
        ikemen::ssz_bridge::refToWstring(pu, file));
}

extern "C" bool SSZ_STDCALL WriteInifileString(PluginUtil* pu,
    Reference str, Reference key, Reference app, Reference file)
{
    return WriteInifileString(
        ikemen::ssz_bridge::refToWstring(pu, str),
        ikemen::ssz_bridge::refToWstring(pu, key),
        ikemen::ssz_bridge::refToWstring(pu, app),
        ikemen::ssz_bridge::refToWstring(pu, file));
}

extern "C" bool SSZ_STDCALL UnCompress(PluginUtil* pu, Reference src, Reference *dst)
{
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
    pu->setSSZFunc();
    dst->releaseanddelete();
    std::wstring output;
    UbytesToStr(src.atpos(), src.len(), cp, output);
    if (output.empty()) return;
    pu->wstrToRef(*dst, output);
}

extern "C" void SSZ_STDCALL StrToUbytes(PluginUtil* pu, Reference src, Reference *dst, UINT cp)
{
    pu->setSSZFunc();
    dst->releaseanddelete();
    std::vector<uint8_t> output;
    StrToUbytes(src.atpos(), src.len(), cp, output);
    if (output.empty()) return;
    ikemen::ssz_bridge::vectorToRefBytes(output, dst);
}

extern "C" void SSZ_STDCALL AsciiToLocal(PluginUtil* pu, Reference src, Reference *dst)
{
    pu->setSSZFunc();
    dst->releaseanddelete();
    std::wstring output;
    AsciiToLocal(src.atpos(), src.len(), output);
    if (output.empty()) return;
    pu->wstrToRef(*dst, output);
}

extern "C" void SSZ_STDCALL SetSharedString(PluginUtil* pu, Reference str)
{
    SetSharedString(ikemen::ssz_bridge::refToWstring(pu, str));
}

extern "C" void SSZ_STDCALL GetSharedString(PluginUtil* pu, Reference *str)
{
    pu->setSSZFunc();
    std::wstring result = GetSharedString();
    str->releaseanddelete();
    pu->wstrToRef(*str, result);
}

extern "C" void SSZ_STDCALL InputStr(PluginUtil* pu, Reference *pr, Reference title)
{
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
    (void)pu;
    return NewClient();
}

extern "C" void SSZ_STDCALL DeleteClient(PluginUtil* pu, Client* client)
{
    (void)pu;
    DeleteClient(client);
}

extern "C" bool SSZ_STDCALL ClientStart(PluginUtil* pu, Client* client)
{
    (void)pu;
    return ClientStart(client);
}

extern "C" bool SSZ_STDCALL ClientStop(PluginUtil* pu, Client* client)
{
    (void)pu;
    return ClientStop(client);
}

extern "C" bool SSZ_STDCALL ClientBufferReady(PluginUtil* pu, Client* client)
{
    (void)pu;
    return ClientBufferReady(client);
}

extern "C" bool SSZ_STDCALL ClientSetBuffer(PluginUtil* pu, Reference src, Client* client)
{
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
    (void)pu;
    return NewOggVorbis();
}

extern "C" void SSZ_STDCALL DeleteOggVorbis(PluginUtil* pu, OggVorbis* ov)
{
    (void)pu;
    DeleteOggVorbis(ov);
}

extern "C" bool SSZ_STDCALL OggVorbisOpen(PluginUtil* pu, Reference file, OggVorbis* ov)
{
    return OggVorbisOpen(ikemen::ssz_bridge::refToWstring(pu, file), ov);
}

extern "C" void SSZ_STDCALL OggVorbisClear(PluginUtil* pu, OggVorbis* ov)
{
    (void)pu;
    OggVorbisClear(ov);
}

extern "C" int64_t SSZ_STDCALL OggVorbisPcmTotal(PluginUtil* pu, OggVorbis* ov)
{
    (void)pu;
    return OggVorbisPcmTotal(ov);
}

extern "C" int32_t SSZ_STDCALL OggVorbisChannels(PluginUtil* pu, OggVorbis* ov)
{
    (void)pu;
    return OggVorbisChannels(ov);
}

extern "C" int32_t SSZ_STDCALL OggVorbisRate(PluginUtil* pu, OggVorbis* ov)
{
    (void)pu;
    return OggVorbisRate(ov);
}

extern "C" intptr_t SSZ_STDCALL OggVorbisRead(PluginUtil* pu, Reference buffer, OggVorbis* ov)
{
    (void)pu;
    return OggVorbisRead((int16_t*)buffer.atpos(), (intptr_t)(buffer.len() / sizeof(int16_t)), ov);
}

extern "C" int32_t SSZ_STDCALL OggVorbisSeek(PluginUtil* pu, double time, OggVorbis* ov)
{
    (void)pu;
    return OggVorbisSeek(time, ov);
}

// =========================================================================
// Regex wrappers — old ABI -> native C++
// =========================================================================

extern "C" RNS::wregex* SSZ_STDCALL NewRegex(PluginUtil* pu, Reference* error, bool i, Reference ptn)
{
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
    (void)pu;
    DeleteRegex(re);
}

extern "C" void SSZ_STDCALL RegexSearch(PluginUtil* pu, Reference* matches, Reference str, RNS::wregex* re)
{
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
    return ShellOpen(
        act, wait,
        ikemen::ssz_bridge::refToWstring(pu, direct),
        ikemen::ssz_bridge::refToWstring(pu, param),
        ikemen::ssz_bridge::refToWstring(pu, file));
}

extern "C" bool SSZ_STDCALL MoveTrash(PluginUtil* pu, Reference file)
{
    return MoveTrash(ikemen::ssz_bridge::refToWstring(pu, file));
}

// =========================================================================
// Thread wrappers — old ABI -> native C++
// =========================================================================

extern "C" void SSZ_STDCALL ThreadDelay(PluginUtil* pu, uint32_t ui)
{
    (void)pu;
    ThreadDelay(ui);
}

// =========================================================================
// Alert wrappers — old ABI -> native C++
// =========================================================================

extern "C" void SSZ_STDCALL Alert(PluginUtil* pu, Reference title, Reference mes)
{
    Alert(
        ikemen::ssz_bridge::refToWstring(pu, title),
        ikemen::ssz_bridge::refToWstring(pu, mes));
}

// =========================================================================
// Time wrappers — old ABI -> native C++
// =========================================================================

extern "C" uint32_t SSZ_STDCALL TickCount(PluginUtil* pu)
{
    (void)pu;
    return TickCount();
}

extern "C" int64_t SSZ_STDCALL UnixTime(PluginUtil* pu)
{
    (void)pu;
    return UnixTime();
}

// =========================================================================
// Math wrappers — old ABI -> native C++
// =========================================================================

extern "C" double SSZ_STDCALL Sin(PluginUtil* pu, double x)
{
    (void)pu;
    return Sin(x);
}

extern "C" double SSZ_STDCALL Cos(PluginUtil* pu, double x)
{
    (void)pu;
    return Cos(x);
}

extern "C" double SSZ_STDCALL Tan(PluginUtil* pu, double x)
{
    (void)pu;
    return Tan(x);
}

extern "C" double SSZ_STDCALL ASin(PluginUtil* pu, double x)
{
    (void)pu;
    return ASin(x);
}

extern "C" double SSZ_STDCALL ACos(PluginUtil* pu, double x)
{
    (void)pu;
    return ACos(x);
}

extern "C" double SSZ_STDCALL ATan(PluginUtil* pu, double x)
{
    (void)pu;
    return ATan(x);
}

extern "C" double SSZ_STDCALL Log(PluginUtil* pu, double y, double x)
{
    (void)pu;
    return Log(y, x);
}

extern "C" double SSZ_STDCALL Ln(PluginUtil* pu, double x)
{
    (void)pu;
    return Ln(x);
}

extern "C" double SSZ_STDCALL Exp(PluginUtil* pu, double x)
{
    (void)pu;
    return Exp(x);
}

extern "C" double SSZ_STDCALL Sqrt(PluginUtil* pu, double x)
{
    (void)pu;
    return Sqrt(x);
}

extern "C" double SSZ_STDCALL Ceil(PluginUtil* pu, double x)
{
    (void)pu;
    return Ceil(x);
}

extern "C" double SSZ_STDCALL Floor(PluginUtil* pu, double x)
{
    (void)pu;
    return Floor(x);
}

extern "C" bool SSZ_STDCALL IsFinite(PluginUtil* pu, double x)
{
    (void)pu;
    return IsFinite(x);
}

extern "C" bool SSZ_STDCALL IsInf(PluginUtil* pu, double x)
{
    (void)pu;
    return IsInf(x);
}

extern "C" bool SSZ_STDCALL IsNaN(PluginUtil* pu, double x)
{
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
    DrawTTF(
        alpha, b, g, r, scaleY, scaleX, y, x,
        ikemen::ssz_bridge::refToWstring(pu, text),
        align,
        ikemen::ssz_bridge::refToWstring(pu, fontPath));
}

extern "C" bool SSZ_STDCALL Init(PluginUtil* pu, bool mugen, int32_t h, int32_t w, Reference cap)
{
    return Init(mugen, h, w, ikemen::ssz_bridge::refToWstring(pu, cap));
}

extern "C" bool SSZ_STDCALL GlInit(PluginUtil* pu, int32_t h, int32_t w, Reference cap)
{
    return GlInit(h, w, ikemen::ssz_bridge::refToWstring(pu, cap));
}

extern "C" bool SSZ_STDCALL RendererInit(PluginUtil* pu, Reference rendererName, int32_t h, int32_t w, Reference cap)
{
    return RendererInit(
        ikemen::ssz_bridge::refToWstring(pu, rendererName),
        h, w,
        ikemen::ssz_bridge::refToWstring(pu, cap));
}

extern "C" void SSZ_STDCALL GetRendererInfo(PluginUtil* pu, Reference* outInfo)
{
    (void)pu;
    (void)outInfo;
    GetRendererInfo();
}

extern "C" void SSZ_STDCALL EnablePerfMonitor(PluginUtil* pu, bool enable)
{
    (void)pu;
    EnablePerfMonitor(enable);
}

extern "C" void SSZ_STDCALL End(PluginUtil* pu)
{
    (void)pu;
    End();
}

extern "C" void SSZ_STDCALL FullScreenExclusive(PluginUtil* pu, bool fsr)
{
    (void)pu;
    FullScreenExclusive(fsr);
}

extern "C" bool SSZ_STDCALL FullScreen(PluginUtil* pu, bool fs)
{
    (void)pu;
    return FullScreen(fs);
}

extern "C" void SSZ_STDCALL WindowType(PluginUtil* pu, int state)
{
    (void)pu;
    WindowType(state);
}

extern "C" int SSZ_STDCALL GetWidth(PluginUtil* pu)
{
    (void)pu;
    return GetWidth();
}

extern "C" int SSZ_STDCALL GetHeight(PluginUtil* pu)
{
    (void)pu;
    return GetHeight();
}

extern "C" void SSZ_STDCALL WindowSize(PluginUtil* pu, int height, int width)
{
    (void)pu;
    WindowSize(height, width);
}

extern "C" void SSZ_STDCALL AspectRatio(PluginUtil* pu, bool aspect)
{
    (void)pu;
    AspectRatio(aspect);
}

extern "C" void SSZ_STDCALL SetOpacity(PluginUtil* pu, float wo)
{
    (void)pu;
    SetOpacity(wo);
}

extern "C" void SSZ_STDCALL TakeScreenShot(PluginUtil* pu, Reference dir)
{
    TakeScreenShot(ikemen::ssz_bridge::refToWstring(pu, dir));
}

extern "C" bool SSZ_STDCALL UpdateGLViewport(PluginUtil* pu, const SDL_Event& event)
{
    (void)pu;
    return UpdateGLViewport(event);
}

extern "C" int SSZ_STDCALL PlayVideo(PluginUtil* pu, Reference fn, Reference screenshotPath, int volume, int audioTrack)
{
    return PlayVideo(
        ikemen::ssz_bridge::refToWstring(pu, fn),
        ikemen::ssz_bridge::refToWstring(pu, screenshotPath),
        volume, audioTrack);
}

extern "C" bool SSZ_STDCALL PollEvent(PluginUtil* pu, int8_t* pb)
{
    (void)pu;
    return PollEvent(pb);
}

extern "C" char16_t SSZ_STDCALL GetLastChar(PluginUtil* pu)
{
    (void)pu;
    return GetLastChar();
}

extern "C" bool SSZ_STDCALL KeyState(PluginUtil* pu, int32_t key)
{
    (void)pu;
    return KeyState(key);
}

extern "C" bool SSZ_STDCALL JoystickButtonState(PluginUtil* pu, int32_t btn, int32_t joy)
{
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
    (void)pu;
    return PollInputBitmask(
        jn, u, d, l, r, a, b, c, x, y, z, q, w, e, s,
        jn2, u2, d2, l2, r2, a2, b2, c2, x2, y2, z2, q2, w2, e2, s2,
        sec);
}

extern "C" void SSZ_STDCALL SoftFill(PluginUtil* pu, uint32_t color, SDL_Rect* prect)
{
    (void)pu;
    SoftFill(color, prect);
}

extern "C" void SSZ_STDCALL Fill(PluginUtil* pu, uint32_t color, SDL_Rect* prect)
{
    (void)pu;
    Fill(color, prect);
}

extern "C" intptr_t SSZ_STDCALL IMGLoad(PluginUtil* pu, Reference fn)
{
    return IMGLoad(ikemen::ssz_bridge::refToWstring(pu, fn));
}

extern "C" void SSZ_STDCALL DecodePNG8(PluginUtil* pu, FILE* fp, int32_t* h, int32_t* w, Reference* out)
{
    pu->setSSZFunc();
    std::vector<uint8_t> decoded;
    DecodePNG8(fp, h, w, decoded);
    ikemen::ssz_bridge::vectorToRefBytes(decoded, out);
}

extern "C" void SSZ_STDCALL BlitSurface(PluginUtil* pu, SDL_Rect* prect, SDL_Surface* psrcs)
{
    (void)pu;
    BlitSurface(prect, psrcs);
}

extern "C" intptr_t SSZ_STDCALL CreatePaletteSurface(PluginUtil* pu, int32_t h, int32_t w, SDL_Color* ppl, uint8_t* ppx)
{
    (void)pu;
    return CreatePaletteSurface(h, w, ppl, ppx);
}

extern "C" void SSZ_STDCALL SetColorKey(PluginUtil* pu, uint32_t key, SDL_Surface* psur)
{
    (void)pu;
    SetColorKey(key, psur);
}

extern "C" void SSZ_STDCALL Flip(PluginUtil* pu)
{
    (void)pu;
    Flip();
}

extern "C" intptr_t SSZ_STDCALL AllocSurface(PluginUtil* pu, int32_t h, int32_t w)
{
    (void)pu;
    return AllocSurface(h, w);
}

extern "C" void SSZ_STDCALL FreeSurface(PluginUtil* pu, SDL_Surface* ps)
{
    (void)pu;
    FreeSurface(ps);
}

extern "C" void SSZ_STDCALL Delay(PluginUtil* pu, uint32_t ms)
{
    (void)pu;
    Delay(ms);
}

extern "C" uint32_t SSZ_STDCALL GetTicks(PluginUtil* pu)
{
    (void)pu;
    return GetTicks();
}

extern "C" void SSZ_STDCALL CursorShow(PluginUtil* pu, bool show)
{
    (void)pu;
    CursorShow(show);
}

extern "C" intptr_t SSZ_STDCALL OpenFont(PluginUtil* pu, int32_t size, Reference font)
{
    return OpenFont(size, ikemen::ssz_bridge::refToWstring(pu, font));
}

extern "C" void SSZ_STDCALL CloseFont(PluginUtil* pu, TTF_Font* pf)
{
    (void)pu;
    CloseFont(pf);
}

extern "C" void SSZ_STDCALL RenderFont(PluginUtil* pu, Reference str, int32_t y, int32_t x, SDL_Color c, TTF_Font* pf)
{
    RenderFont(ikemen::ssz_bridge::refToWstring(pu, str), y, x, c, pf);
}

extern "C" bool SSZ_STDCALL SetSndBuf(PluginUtil* pu, int32_t* buf)
{
    (void)pu;
    return SetSndBuf(buf);
}

extern "C" bool SSZ_STDCALL PlayBGM(PluginUtil* pu, Reference fn, Reference pldir)
{
    return PlayBGM(
        ikemen::ssz_bridge::refToWstring(pu, fn),
        ikemen::ssz_bridge::refToWstring(pu, pldir));
}

extern "C" void SSZ_STDCALL PauseBGM(PluginUtil* pu, bool pause)
{
    (void)pu;
    PauseBGM(pause);
}

extern "C" bool SSZ_STDCALL SendOpenBGM(PluginUtil* pu, int32_t channels, int32_t rate)
{
    (void)pu;
    return SendOpenBGM(channels, rate);
}

extern "C" void SSZ_STDCALL SendCloseBGM(PluginUtil* pu)
{
    (void)pu;
    SendCloseBGM();
}

extern "C" intptr_t SSZ_STDCALL SendWriteBGM(PluginUtil* pu, Reference fn)
{
    (void)pu;
    (void)fn;
    return SendWriteBGM();
}

extern "C" void SSZ_STDCALL SetVolume(PluginUtil* pu, float bv, float wv, float gv)
{
    (void)pu;
    SetVolume(bv, wv, gv);
}

extern "C" void SSZ_STDCALL FadeInBGM(PluginUtil* pu, int time)
{
    (void)pu;
    FadeInBGM(time);
}

extern "C" void SSZ_STDCALL FadeOutBGM(PluginUtil* pu, int time)
{
    (void)pu;
    FadeOutBGM(time);
}

extern "C" bool SSZ_STDCALL RenderMugenZoom(PluginUtil* pu, Reference* pluginbuf, int32_t rle,
    float rcy, float rcx, SDL_Rect* pdstr, int32_t alpha,
    uint32_t roto, float rasterxadd, float yscl, float xbotscl, float xtopscl,
    SDL_Rect* tile, float ty, float cx, SDL_Rect* psrcr,
    uint16_t ckey, uint32_t* ppal, Reference img)
{
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
    (void)pu;
    return RenderFontBatch(count, glyphData, spacing, yscl, xscl, window,
        alpha, glyphH, atlasStride, ppal, baseY, baseX, atlasPixels);
}

extern "C" bool SSZ_STDCALL RenderMugenShadow(PluginUtil* pu, Reference* pluginbuf, int32_t rle,
    float rcy, float rcx, SDL_Rect* pdstr, int32_t alpha,
    uint32_t roto, float vscl, float yscl, float xscl,
    float ty, float cx, SDL_Rect* psrcr, uint32_t color, Reference img)
{
    return RenderMugenShadow(pluginbuf, rle, rcy, rcx, pdstr, alpha,
        roto, vscl, yscl, xscl, ty, cx, psrcr, color, img);
}

extern "C" uint32_t SSZ_STDCALL Load8bitTexture(PluginUtil* pu, int32_t h, int32_t w, uint8_t* ppxl)
{
    (void)pu;
    return Load8bitTexture(h, w, ppxl);
}

extern "C" uint32_t SSZ_STDCALL LoadPngTexture(PluginUtil* pu, FILE* fp, int32_t* h, int32_t* w)
{
    (void)pu;
    return LoadPngTexture(fp, h, w);
}

extern "C" void SSZ_STDCALL DeleteGlTexture(PluginUtil* pu, uint32_t texid)
{
    (void)pu;
    DeleteGlTexture(texid);
}

extern "C" void SSZ_STDCALL GlSwapBuffers(PluginUtil* pu)
{
    (void)pu;
    GlSwapBuffers();
}

extern "C" bool SSZ_STDCALL InitMugenGl(PluginUtil* pu)
{
    (void)pu;
    return InitMugenGl();
}

extern "C" bool SSZ_STDCALL RenderMugenGl(PluginUtil* pu, float rcy, float rcx, SDL_Rect* dstr, int alpha,
    float angle, float rasterxadd, float vscl, float yscl,
    float xbotscl, float xtopscl, SDL_Rect* tile, float y, float x,
    SDL_Rect* rect, int mask, uint8_t* ppal, uint32_t texid)
{
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
    (void)pu;
    return RenderMugenGlFcS(color, rcy, rcx, dstr, alpha, angle, rasterxadd,
        vscl, yscl, xbotscl, xtopscl, tile, y, x, rect, texid);
}

extern "C" void SSZ_STDCALL MugenFillGl(PluginUtil* pu, int32_t alpha, uint32_t color, SDL_Rect rect)
{
    (void)pu;
    MugenFillGl(alpha, color, rect);
}

extern "C" bool SSZ_STDCALL BindGlContext(PluginUtil* pu)
{
    (void)pu;
    return BindGlContext();
}

extern "C" bool SSZ_STDCALL UnbindGlContext(PluginUtil* pu)
{
    (void)pu;
    return UnbindGlContext();
}
