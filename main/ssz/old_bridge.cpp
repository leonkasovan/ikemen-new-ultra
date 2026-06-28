// bridge.cpp — Old ABI wrappers for SSZ JIT compatibility
// The SSZ JIT calls plugin functions with the old calling convention
// (PluginUtil* first arg, Reference strings). These wrappers convert
// parameters to the new native types and forward to the refactored plugins.

#include "sszdef.h"
#include "typeid.h"
#include "arrayandref.hpp"
#include "pluginutil.hpp"
#ifdef _WIN32
#include <winsock2.h>
#include <regex>
#define RNS std
#else
#include <boost/regex.hpp>
#define RNS boost
typedef int SOCKET;
#endif

// Forward declarations for opaque plugin types
class Client;
struct OggVorbis;
class CompilerState;
struct lua_State;

// SDL types — full includes needed for:
//   - pass-by-value (SDL_Color, SDL_Rect)
//   - proper type equivalence with plugin implementations
//     (TTF_Font is a typedef for struct _TTF_Font — a C++
//      forward-decl `struct TTF_Font;` would create a distinct
//      incomplete type, breaking name mangling across TUs)
#include <SDL.h>
#include <SDL_ttf.h>

// =========================================================================
// Forward declarations for new ABI plugin functions (C++ linkage)
// These are defined in their respective plugin .cpp files.
// =========================================================================

// --- math (15) ---
double SSZ_STDCALL Sin(double);
double SSZ_STDCALL Cos(double);
double SSZ_STDCALL Tan(double);
double SSZ_STDCALL ASin(double);
double SSZ_STDCALL ACos(double);
double SSZ_STDCALL ATan(double);
double SSZ_STDCALL Log(double, double);
double SSZ_STDCALL Ln(double);
double SSZ_STDCALL Exp(double);
double SSZ_STDCALL Sqrt(double);
double SSZ_STDCALL Ceil(double);
double SSZ_STDCALL Floor(double);
bool   SSZ_STDCALL IsFinite(double);
bool   SSZ_STDCALL IsInf(double);
bool   SSZ_STDCALL IsNaN(double);

// --- thread (1) ---
void   SSZ_STDCALL ThreadDelay(uint32_t);

// --- time (2) ---
uint32_t SSZ_STDCALL TickCount();
int64_t  SSZ_STDCALL UnixTime();

// --- alert (1) ---
void   SSZ_STDCALL Alert(const std::wstring&, const std::wstring&);

// --- shell (2) ---
bool   SSZ_STDCALL ShellOpen(bool, bool, const std::wstring&, const std::wstring&, const std::wstring&);
bool   SSZ_STDCALL MoveTrash(const std::wstring&);

// --- sound (6) ---
Client* SSZ_STDCALL NewClient();
void    SSZ_STDCALL DeleteClient(Client*);
bool    SSZ_STDCALL ClientStart(Client*);
bool    SSZ_STDCALL ClientStop(Client*);
bool    SSZ_STDCALL ClientBufferReady(Client*);
bool    SSZ_STDCALL ClientSetBuffer(const float*, intptr_t, Client*);

// --- socket (8) ---
void     SSZ_STDCALL SocketClose(SOCKET*);
bool     SSZ_STDCALL SocketConnect(bool, int32_t, const std::wstring&, const std::wstring&, SOCKET*);
bool     SSZ_STDCALL SocketListen(bool, int32_t, const std::wstring&, SOCKET*);
SOCKET   SSZ_STDCALL SocketAccept(bool, int32_t, SOCKET);
bool     SSZ_STDCALL SocketSend(intptr_t, const char*, SOCKET*);
intptr_t SSZ_STDCALL SocketSendAry(intptr_t, const void*, intptr_t, SOCKET*);
bool     SSZ_STDCALL SocketRecv(intptr_t, char*, SOCKET*);
intptr_t SSZ_STDCALL SocketRecvAry(intptr_t, void*, intptr_t, SOCKET*);

// --- ogg (9) ---
OggVorbis* SSZ_STDCALL NewOggVorbis();
void       SSZ_STDCALL DeleteOggVorbis(OggVorbis*);
bool       SSZ_STDCALL OggVorbisOpen(const std::wstring&, OggVorbis*);
void       SSZ_STDCALL OggVorbisClear(OggVorbis*);
int64_t    SSZ_STDCALL OggVorbisPcmTotal(OggVorbis*);
int32_t    SSZ_STDCALL OggVorbisChannels(OggVorbis*);
int32_t    SSZ_STDCALL OggVorbisRate(OggVorbis*);
intptr_t   SSZ_STDCALL OggVorbisRead(int16_t*, intptr_t, OggVorbis*);
int32_t    SSZ_STDCALL OggVorbisSeek(double, OggVorbis*);

// --- regex (3) ---
RNS::wregex* SSZ_STDCALL NewRegex(std::wstring*, bool, const std::wstring&);
void         SSZ_STDCALL DeleteRegex(RNS::wregex*);
void         SSZ_STDCALL RegexSearch(std::vector<std::wstring>*, const std::wstring&, RNS::wregex*);

// --- file (18) ---
intptr_t SSZ_STDCALL Open(const std::wstring&, const std::wstring&);
void     SSZ_STDCALL FileClose(FILE*);
bool     SSZ_STDCALL Read(intptr_t, void*, FILE*);
intptr_t SSZ_STDCALL ReadAry(intptr_t, void*, intptr_t, FILE*);
bool     SSZ_STDCALL Write(intptr_t, const void*, FILE*);
intptr_t SSZ_STDCALL WriteAry(intptr_t, const void*, intptr_t, FILE*);
bool     SSZ_STDCALL Seek(int32_t, int64_t, FILE*);
void     SSZ_STDCALL LoadAsciiText(std::wstring*, const std::wstring&);
bool     SSZ_STDCALL SaveAsciiText(const std::wstring&, const std::wstring&);
bool     SSZ_STDCALL Delete(const std::wstring&);
bool     SSZ_STDCALL Move(const std::wstring&, const std::wstring&);
bool     SSZ_STDCALL Copy(bool, const std::wstring&, const std::wstring&);
void     SSZ_STDCALL Find(std::vector<std::wstring>*, const std::wstring&);
void     SSZ_STDCALL FindDir(std::vector<std::wstring>*, const std::wstring&);
bool     SSZ_STDCALL CreateDir(const std::wstring&);
bool     SSZ_STDCALL RemoveDir(const std::wstring&);
bool     SSZ_STDCALL SetCurrentDir(const std::wstring&);
void     SSZ_STDCALL GetCurrentDir(std::wstring*);

// --- mesdialog (15) ---
bool       SSZ_STDCALL YesNo(const std::wstring&);
void       SSZ_STDCALL VeryUnsafeCopy(intptr_t, void*, void*);
bool       SSZ_STDCALL GetClipboardStr(std::wstring*);
intptr_t   SSZ_STDCALL TazyuuCheck(const std::wstring&);
void       SSZ_STDCALL CloseTazyuuHandle(intptr_t);
void       SSZ_STDCALL GetInifileString(std::wstring*, const std::wstring&, const std::wstring&, const std::wstring&, const std::wstring&);
int32_t    SSZ_STDCALL GetInifileInt(int32_t, const std::wstring&, const std::wstring&, const std::wstring&);
bool       SSZ_STDCALL WriteInifileString(const std::wstring&, const std::wstring&, const std::wstring&, const std::wstring&);
bool       SSZ_STDCALL UnCompress(const void*, intptr_t, std::string*);
void       SSZ_STDCALL UbytesToStr(const void*, intptr_t, std::wstring*, UINT);
void       SSZ_STDCALL StrToUbytes(const void*, intptr_t, std::string*, UINT);
void       SSZ_STDCALL AsciiToLocal(const std::wstring&, std::wstring*);
void       SSZ_STDCALL SetSharedString(const std::wstring&);
void       SSZ_STDCALL GetSharedString(std::wstring*);
void       SSZ_STDCALL InputStr(std::wstring*, const std::wstring&);

// --- lua (22) ---
void       SSZ_STDCALL LuaInit(intptr_t, intptr_t);
lua_State* SSZ_STDCALL NewState();
void       SSZ_STDCALL Close(lua_State*);
bool       SSZ_STDCALL RunFile(const std::string&, lua_State*);
bool       SSZ_STDCALL RunString(const std::string&, lua_State*);
int32_t    SSZ_STDCALL GetTop(lua_State*);
void       SSZ_STDCALL GetGlobal(const std::string&, lua_State*);
void       SSZ_STDCALL Register(intptr_t, const std::string&, lua_State*);
bool       SSZ_STDCALL Pcall(int32_t, int32_t, lua_State*);
void       SSZ_STDCALL Pop(int32_t, lua_State*);
void       SSZ_STDCALL PushNumber(double, lua_State*);
bool       SSZ_STDCALL IsNumber(int32_t, lua_State*);
double     SSZ_STDCALL ToNumber(int32_t, lua_State*);
void       SSZ_STDCALL PushBoolean(bool, lua_State*);
bool       SSZ_STDCALL IsBoolean(int32_t, lua_State*);
bool       SSZ_STDCALL ToBoolean(int32_t, lua_State*);
void       SSZ_STDCALL PushString(const std::string&, lua_State*);
bool       SSZ_STDCALL IsString(int32_t, lua_State*);
void       SSZ_STDCALL ToString(int32_t, std::string*, lua_State*);
void       SSZ_STDCALL PushRef(DynamicRef*, lua_State*);
void       SSZ_STDCALL ToRef(int32_t, DynamicRef*, lua_State*);

// --- ssz (9) ---
bool            SSZ_STDCALL Run(const std::wstring&);
CompilerState*  SSZ_STDCALL NewCompiler();
void            SSZ_STDCALL DeleteCompiler(CompilerState*);
void            SSZ_STDCALL CompilerCompile(std::wstring*, const std::wstring&, CompilerState*);
void            SSZ_STDCALL CompilerCompileString(std::wstring*, const std::wstring&, const std::wstring&, CompilerState*);
bool            SSZ_STDCALL CompilerRun(CompilerState*);
void            SSZ_STDCALL MemMarkBefore(const std::wstring&);
void            SSZ_STDCALL MemMarkAfter(const std::wstring&);
PluginSSZFuncs* SSZ_STDCALL GetSSZFuncs();

// --- sdlplugin (62) ---
bool      SSZ_STDCALL Init(bool, int32_t, int32_t, const std::string&);
bool      SSZ_STDCALL GlInit(int32_t, int32_t, const std::string&);
bool      SSZ_STDCALL RendererInit(const std::string&, int32_t, int32_t, const std::string&);
void      SSZ_STDCALL End();
void      SSZ_STDCALL GetRendererInfo(std::string*);
void      SSZ_STDCALL EnablePerfMonitor(bool);
void      SSZ_STDCALL FullScreenExclusive(bool);
bool      SSZ_STDCALL FullScreen(bool);
void      SSZ_STDCALL WindowType(int);
int       SSZ_STDCALL GetWidth();
int       SSZ_STDCALL GetHeight();
void      SSZ_STDCALL WindowSize(int, int);
void      SSZ_STDCALL AspectRatio(bool);
void      SSZ_STDCALL SetOpacity(float);
void      SSZ_STDCALL TakeScreenShot(const std::string&);
void      SSZ_STDCALL Flip();
bool      SSZ_STDCALL UpdateGLViewport(const SDL_Event&);
bool      SSZ_STDCALL PollEvent(int8_t*);
char16_t  SSZ_STDCALL GetLastChar();
bool      SSZ_STDCALL KeyState(int32_t);
bool      SSZ_STDCALL JoystickButtonState(int32_t, int32_t);
int32_t   SSZ_STDCALL PollInputBitmask(int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t);
void      SSZ_STDCALL DrawTTF(int32_t, int32_t, int32_t, int32_t, float, float, int32_t, int32_t, const std::wstring&, int32_t, const std::wstring&);
void      SSZ_STDCALL SoftFill(uint32_t, SDL_Rect*);
void      SSZ_STDCALL Fill(uint32_t, SDL_Rect*);
intptr_t  SSZ_STDCALL IMGLoad(const std::string&);
void      SSZ_STDCALL DecodePNG8(FILE*, int32_t*, int32_t*, std::vector<uint8_t>*);
void      SSZ_STDCALL BlitSurface(SDL_Rect*, SDL_Surface*);
intptr_t  SSZ_STDCALL CreatePaletteSurface(int32_t, int32_t, SDL_Color*, uint8_t*);
void      SSZ_STDCALL SetColorKey(uint32_t, SDL_Surface*);
intptr_t  SSZ_STDCALL AllocSurface(int32_t, int32_t);
void      SSZ_STDCALL FreeSurface(SDL_Surface*);
void      SSZ_STDCALL Delay(uint32_t);
uint32_t  SSZ_STDCALL GetTicks();
void      SSZ_STDCALL CursorShow(bool);
intptr_t  SSZ_STDCALL OpenFont(int32_t, const std::string&);
void      SSZ_STDCALL CloseFont(TTF_Font*);
void      SSZ_STDCALL RenderFont(const std::wstring&, int32_t, int32_t, SDL_Color, TTF_Font*);
bool      SSZ_STDCALL RenderFontBatch(int32_t, int32_t*, float, float, float, SDL_Rect*, int32_t, int32_t, int32_t, uint32_t*, float, float, uint8_t*);
bool      SSZ_STDCALL SetSndBuf(int32_t*);
bool      SSZ_STDCALL PlayBGM(const std::wstring&, const std::wstring&);
void      SSZ_STDCALL PauseBGM(bool);
bool      SSZ_STDCALL SendOpenBGM(int32_t, int32_t);
void      SSZ_STDCALL SendCloseBGM();
intptr_t  SSZ_STDCALL SendWriteBGM(const void*, intptr_t);
void      SSZ_STDCALL SetVolume(float, float, float);
void      SSZ_STDCALL FadeInBGM(int);
void      SSZ_STDCALL FadeOutBGM(int);
int       SSZ_STDCALL PlayVideo(const std::wstring&, const std::wstring&, int32_t, int32_t);
bool      SSZ_STDCALL RenderMugenZoom(uint8_t*, intptr_t, int32_t, float, float, SDL_Rect*, int32_t, uint32_t, float, float, float, float, SDL_Rect*, float, float, SDL_Rect*, uint16_t, uint32_t*, const uint8_t*, intptr_t);
bool      SSZ_STDCALL RenderMugenShadow(uint8_t*, intptr_t, int32_t, float, float, SDL_Rect*, int32_t, uint32_t, float, float, float, float, float, SDL_Rect*, uint32_t, const uint8_t*, intptr_t);
uint32_t  SSZ_STDCALL Load8bitTexture(int32_t, int32_t, uint8_t*);
uint32_t  SSZ_STDCALL LoadPngTexture(FILE*, int32_t*, int32_t*);
void      SSZ_STDCALL DeleteGlTexture(uint32_t);
void      SSZ_STDCALL GlSwapBuffers();
bool      SSZ_STDCALL InitMugenGl();
bool      SSZ_STDCALL RenderMugenGl(float, float, SDL_Rect*, int, float, float, float, float, float, float, SDL_Rect*, float, float, SDL_Rect*, int, uint8_t*, uint32_t);
bool      SSZ_STDCALL RenderMugenGlFc(float, float, float, float, float, float, float, bool, float, float, SDL_Rect*, int, float, float, float, float, float, float, SDL_Rect*, float, float, SDL_Rect*, uint32_t);
bool      SSZ_STDCALL RenderMugenGlFcS(uint32_t, float, float, SDL_Rect*, int, float, float, float, float, float, float, SDL_Rect*, float, float, SDL_Rect*, uint32_t);
void      SSZ_STDCALL MugenFillGl(int32_t, uint32_t, SDL_Rect);
bool      SSZ_STDCALL BindGlContext();
bool      SSZ_STDCALL UnbindGlContext();

// =========================================================================
// Helper macros for Reference-to-string conversion
// =========================================================================
#define W(r)     PluginUtil::refToWstr(r)
#define A_U8(r)  PluginUtil::refToAstr(CP_UTF8, r)
#define A_ACP(r) PluginUtil::refToAstr(CP_THREAD_ACP, r)

// =========================================================================
// Bridge wrapper functions (old ABI → new ABI)
// =========================================================================

extern "C" {
// --- math (15) ---
double SSZ_STDCALL Sin(PluginUtil*, double x) { return Sin(x); }
double SSZ_STDCALL Cos(PluginUtil*, double x) { return Cos(x); }
double SSZ_STDCALL Tan(PluginUtil*, double x) { return Tan(x); }
double SSZ_STDCALL ASin(PluginUtil*, double x) { return ASin(x); }
double SSZ_STDCALL ACos(PluginUtil*, double x) { return ACos(x); }
double SSZ_STDCALL ATan(PluginUtil*, double x) { return ATan(x); }
double SSZ_STDCALL Log(PluginUtil*, double y, double x) { return Log(y, x); }
double SSZ_STDCALL Ln(PluginUtil*, double x) { return Ln(x); }
double SSZ_STDCALL Exp(PluginUtil*, double x) { return Exp(x); }
double SSZ_STDCALL Sqrt(PluginUtil*, double x) { return Sqrt(x); }
double SSZ_STDCALL Ceil(PluginUtil*, double x) { return Ceil(x); }
double SSZ_STDCALL Floor(PluginUtil*, double x) { return Floor(x); }
bool   SSZ_STDCALL IsFinite(PluginUtil*, double x) { return IsFinite(x); }
bool   SSZ_STDCALL IsInf(PluginUtil*, double x) { return IsInf(x); }
bool   SSZ_STDCALL IsNaN(PluginUtil*, double x) { return IsNaN(x); }

// --- thread (1) ---
void SSZ_STDCALL ThreadDelay(PluginUtil*, uint32_t ms) { ThreadDelay(ms); }

// --- time (2) ---
uint32_t SSZ_STDCALL TickCount(PluginUtil*) { return TickCount(); }
int64_t  SSZ_STDCALL UnixTime(PluginUtil*)  { return UnixTime(); }

// --- alert (1) ---
void SSZ_STDCALL Alert(PluginUtil*, Reference title, Reference mes) {
    Alert(W(title), W(mes));
}

// --- shell (2) ---
bool SSZ_STDCALL ShellOpen(PluginUtil*, bool act, bool wait, Reference direct, Reference param, Reference file) {
    return ShellOpen(act, wait, W(direct), W(param), W(file));
}
bool SSZ_STDCALL MoveTrash(PluginUtil*, Reference file) {
    return MoveTrash(W(file));
}

// --- sound (6) ---
Client* SSZ_STDCALL NewClient(PluginUtil*) { return NewClient(); }
void    SSZ_STDCALL DeleteClient(PluginUtil*, Client* c) { DeleteClient(c); }
bool    SSZ_STDCALL ClientStart(PluginUtil*, Client* c) { return ClientStart(c); }
bool    SSZ_STDCALL ClientStop(PluginUtil*, Client* c) { return ClientStop(c); }
bool    SSZ_STDCALL ClientBufferReady(PluginUtil*, Client* c) { return ClientBufferReady(c); }
bool    SSZ_STDCALL ClientSetBuffer(PluginUtil*, Reference src, intptr_t len, Client* c) {
    return ClientSetBuffer((const float*)src.atpos(), len, c);
}

// --- socket (8) ---
void     SSZ_STDCALL SocketClose(PluginUtil*, SOCKET* ps) { SocketClose(ps); }
bool     SSZ_STDCALL SocketConnect(PluginUtil*, bool nd, int32_t t, Reference pt, Reference h, SOCKET* ps) {
    return SocketConnect(nd, t, W(pt), W(h), ps);
}
bool     SSZ_STDCALL SocketListen(PluginUtil*, bool v4, int32_t bl, Reference pt, SOCKET* ps) {
    return SocketListen(v4, bl, W(pt), ps);
}
SOCKET   SSZ_STDCALL SocketAccept(PluginUtil*, bool nd, int32_t t, SOCKET s) { return SocketAccept(nd, t, s); }
bool     SSZ_STDCALL SocketSend(PluginUtil*, intptr_t sz, char* p, SOCKET* ps) { return SocketSend(sz, p, ps); }
intptr_t SSZ_STDCALL SocketSendAry(PluginUtil*, Reference ary, intptr_t sz, SOCKET* ps) {
    return SocketSendAry(sz, ary.atpos(), ary.len(), ps);
}
bool     SSZ_STDCALL SocketRecv(PluginUtil*, intptr_t sz, char* p, SOCKET* ps) { return SocketRecv(sz, p, ps); }
intptr_t SSZ_STDCALL SocketRecvAry(PluginUtil*, Reference ary, intptr_t sz, SOCKET* ps) {
    return SocketRecvAry(sz, ary.atpos(), ary.len(), ps);
}

// --- ogg (9) ---
OggVorbis* SSZ_STDCALL NewOggVorbis(PluginUtil*) { return NewOggVorbis(); }
void       SSZ_STDCALL DeleteOggVorbis(PluginUtil*, OggVorbis* ov) { DeleteOggVorbis(ov); }
bool       SSZ_STDCALL OggVorbisOpen(PluginUtil*, Reference file, OggVorbis* ov) {
    return OggVorbisOpen(W(file), ov);
}
void       SSZ_STDCALL OggVorbisClear(PluginUtil*, OggVorbis* ov) { OggVorbisClear(ov); }
int64_t    SSZ_STDCALL OggVorbisPcmTotal(PluginUtil*, OggVorbis* ov) { return OggVorbisPcmTotal(ov); }
int32_t    SSZ_STDCALL OggVorbisChannels(PluginUtil*, OggVorbis* ov) { return OggVorbisChannels(ov); }
int32_t    SSZ_STDCALL OggVorbisRate(PluginUtil*, OggVorbis* ov) { return OggVorbisRate(ov); }
intptr_t   SSZ_STDCALL OggVorbisRead(PluginUtil*, Reference buf, intptr_t maxsamples, OggVorbis* ov) {
    return OggVorbisRead((int16_t*)buf.atpos(), maxsamples, ov);
}
int32_t    SSZ_STDCALL OggVorbisSeek(PluginUtil*, double t, OggVorbis* ov) { return OggVorbisSeek(t, ov); }

// --- regex (3) ---
RNS::wregex* SSZ_STDCALL NewRegex(PluginUtil*, Reference err, bool i, Reference ptn) {
    std::wstring werr;
    auto re = NewRegex(&werr, i, W(ptn));
    PluginUtil::wstrToRef(err, werr);
    return re;
}
void SSZ_STDCALL DeleteRegex(PluginUtil*, RNS::wregex* re) { DeleteRegex(re); }
void SSZ_STDCALL RegexSearch(PluginUtil*, Reference matches, Reference str, RNS::wregex* re) {
    std::vector<std::wstring> m;
    RegexSearch(&m, W(str), re);
    matches.releaseanddelete();
    matches.refnew(m.size(), sizeof(Reference));
    for(size_t i = 0; i < m.size(); i++){
        ((Reference*)matches.atpos())[i].init();
        PluginUtil::wstrToRef(((Reference*)matches.atpos())[i], m[i]);
    }
}

// --- file (18) ---
intptr_t SSZ_STDCALL Open(PluginUtil*, Reference md, Reference fn) {
    return Open(W(md), W(fn));
}
void     SSZ_STDCALL FileClose(PluginUtil*, FILE* f) { FileClose(f); }
bool     SSZ_STDCALL Read(PluginUtil*, intptr_t sz, Reference buf, FILE* f) {
    return Read(sz, buf.atpos(), f);
}
intptr_t SSZ_STDCALL ReadAry(PluginUtil*, intptr_t sz, Reference buf, intptr_t count, FILE* f) {
    return ReadAry(sz, buf.atpos(), count, f);
}
bool     SSZ_STDCALL Write(PluginUtil*, intptr_t sz, Reference buf, FILE* f) {
    return Write(sz, buf.atpos(), f);
}
intptr_t SSZ_STDCALL WriteAry(PluginUtil*, intptr_t sz, Reference buf, intptr_t count, FILE* f) {
    return WriteAry(sz, buf.atpos(), count, f);
}
bool     SSZ_STDCALL Seek(PluginUtil*, int32_t ori, int64_t off, FILE* f) { return Seek(ori, off, f); }
void     SSZ_STDCALL LoadAsciiText(PluginUtil*, Reference out, Reference path) {
    std::wstring wout;
    LoadAsciiText(&wout, W(path));
    PluginUtil::wstrToRef(out, wout);
}
bool     SSZ_STDCALL SaveAsciiText(PluginUtil*, Reference txt, Reference r) {
    return SaveAsciiText(W(txt), W(r));
}
bool     SSZ_STDCALL Delete(PluginUtil*, Reference f) { return Delete(W(f)); }
bool     SSZ_STDCALL Move(PluginUtil*, Reference n, Reference o) { return Move(W(n), W(o)); }
bool     SSZ_STDCALL Copy(PluginUtil*, bool ov, Reference d, Reference s) { return Copy(ov, W(d), W(s)); }
void     SSZ_STDCALL Find(PluginUtil*, Reference out, Reference pattern) {
    std::vector<std::wstring> results;
    Find(&results, W(pattern));
    out.releaseanddelete();
    out.refnew(results.size(), sizeof(Reference));
    for(size_t i = 0; i < results.size(); i++){
        ((Reference*)out.atpos())[i].init();
        PluginUtil::wstrToRef(((Reference*)out.atpos())[i], results[i]);
    }
}
void SSZ_STDCALL FindDir(PluginUtil*, Reference out, Reference pattern) {
    std::vector<std::wstring> results;
    FindDir(&results, W(pattern));
    out.releaseanddelete();
    out.refnew(results.size(), sizeof(Reference));
    for(size_t i = 0; i < results.size(); i++){
        ((Reference*)out.atpos())[i].init();
        PluginUtil::wstrToRef(((Reference*)out.atpos())[i], results[i]);
    }
}
bool SSZ_STDCALL CreateDir(PluginUtil*, Reference d) { return CreateDir(W(d)); }
bool SSZ_STDCALL RemoveDir(PluginUtil*, Reference d) { return RemoveDir(W(d)); }
bool SSZ_STDCALL SetCurrentDir(PluginUtil*, Reference d) { return SetCurrentDir(W(d)); }
void SSZ_STDCALL GetCurrentDir(PluginUtil*, Reference out) {
    std::wstring wout;
    GetCurrentDir(&wout);
    PluginUtil::wstrToRef(out, wout);
}

// --- mesdialog (15) ---
bool SSZ_STDCALL YesNo(PluginUtil*, Reference r) { return YesNo(W(r)); }void SSZ_STDCALL VeryUnsafeCopy(PluginUtil*, intptr_t sz, Reference s, Reference d) {
    VeryUnsafeCopy(sz, s.atpos(), d.atpos());
}
bool SSZ_STDCALL GetClipboardStr(PluginUtil*, Reference r) {
    std::wstring out;
    bool ok = GetClipboardStr(&out);
    if(ok) PluginUtil::wstrToRef(r, out);
    return ok;
}
intptr_t SSZ_STDCALL TazyuuCheck(PluginUtil*, Reference name) { return TazyuuCheck(W(name)); }
void     SSZ_STDCALL CloseTazyuuHandle(PluginUtil*, intptr_t h) { CloseTazyuuHandle(h); }
void     SSZ_STDCALL GetInifileString(PluginUtil*, Reference p, Reference d, Reference k, Reference a, Reference f) {
    std::wstring out;
    GetInifileString(&out, W(d), W(k), W(a), W(f));
    PluginUtil::wstrToRef(p, out);
}
int32_t SSZ_STDCALL GetInifileInt(PluginUtil*, int32_t d, Reference k, Reference a, Reference f) {
    return GetInifileInt(d, W(k), W(a), W(f));
}
bool SSZ_STDCALL WriteInifileString(PluginUtil*, Reference s, Reference k, Reference a, Reference f) {
    return WriteInifileString(W(s), W(k), W(a), W(f));
}
bool SSZ_STDCALL UnCompress(PluginUtil*, Reference src, intptr_t srclen, Reference dst) {
    std::string out;
    bool ok = UnCompress(src.atpos(), srclen, &out);
    if(ok){
        dst.releaseanddelete();
        dst.refnew(out.size(), 1);
        memcpy(dst.atpos(), out.data(), out.size());
    }
    return ok;
}
void SSZ_STDCALL UbytesToStr(PluginUtil*, Reference src, intptr_t srclen, Reference dst, UINT cp) {
    std::wstring out;
    UbytesToStr(src.atpos(), srclen, &out, cp);
    PluginUtil::wstrToRef(dst, out);
}
void SSZ_STDCALL StrToUbytes(PluginUtil*, Reference src, intptr_t srclen, Reference dst, UINT cp) {
    std::string out;
    StrToUbytes(src.atpos(), srclen, &out, cp);
    dst.releaseanddelete();
    dst.refnew(out.size(), 1);
    memcpy(dst.atpos(), out.data(), out.size());
}
void SSZ_STDCALL AsciiToLocal(PluginUtil*, Reference src, Reference dst) {
    std::wstring out;
    AsciiToLocal(W(src), &out);
    PluginUtil::wstrToRef(dst, out);
}
void SSZ_STDCALL SetSharedString(PluginUtil*, Reference s) { SetSharedString(W(s)); }
void SSZ_STDCALL GetSharedString(PluginUtil*, Reference s) {
    std::wstring out;
    GetSharedString(&out);
    PluginUtil::wstrToRef(s, out);
}
void SSZ_STDCALL InputStr(PluginUtil*, Reference pr, Reference title) {
    std::wstring out;
    InputStr(&out, W(title));
    PluginUtil::wstrToRef(pr, out);
}

// --- lua (22) ---
void       SSZ_STDCALL LuaInit(PluginUtil*, intptr_t rc, intptr_t rd) { LuaInit(rc, rd); }
lua_State* SSZ_STDCALL NewState(PluginUtil*) { return NewState(); }
void       SSZ_STDCALL Close(PluginUtil*, lua_State* L) { Close(L); }
bool       SSZ_STDCALL RunFile(PluginUtil*, Reference f, lua_State* L) { return RunFile(A_U8(f), L); }
bool       SSZ_STDCALL RunString(PluginUtil*, Reference s, lua_State* L) { return RunString(A_U8(s), L); }
int32_t    SSZ_STDCALL GetTop(PluginUtil*, lua_State* L) { return GetTop(L); }
void       SSZ_STDCALL GetGlobal(PluginUtil*, Reference v, lua_State* L) { GetGlobal(A_U8(v), L); }
void       SSZ_STDCALL Register(PluginUtil*, intptr_t f, Reference v, lua_State* L) { Register(f, A_U8(v), L); }
bool       SSZ_STDCALL Pcall(PluginUtil*, int32_t nr, int32_t na, lua_State* L) { return Pcall(nr, na, L); }
void       SSZ_STDCALL Pop(PluginUtil*, int32_t n, lua_State* L) { Pop(n, L); }
void       SSZ_STDCALL PushNumber(PluginUtil*, double n, lua_State* L) { PushNumber(n, L); }
bool       SSZ_STDCALL IsNumber(PluginUtil*, int32_t i, lua_State* L) { return IsNumber(i, L); }
double     SSZ_STDCALL ToNumber(PluginUtil*, int32_t i, lua_State* L) { return ToNumber(i, L); }
void       SSZ_STDCALL PushBoolean(PluginUtil*, bool b, lua_State* L) { PushBoolean(b, L); }
bool       SSZ_STDCALL IsBoolean(PluginUtil*, int32_t i, lua_State* L) { return IsBoolean(i, L); }
bool       SSZ_STDCALL ToBoolean(PluginUtil*, int32_t i, lua_State* L) { return ToBoolean(i, L); }
void       SSZ_STDCALL PushString(PluginUtil*, Reference s, lua_State* L) { PushString(A_U8(s), L); }
bool       SSZ_STDCALL IsString(PluginUtil*, int32_t i, lua_State* L) { return IsString(i, L); }
void       SSZ_STDCALL ToString(PluginUtil*, int32_t i, Reference s, lua_State* L) {
    std::string out;
    ToString(i, &out, L);
    // aToW: std::string → std::wstring
    int _len = MultiByteToWideChar(CP_UTF8, 0, out.c_str(), -1, nullptr, 0);
    std::wstring wout;
    if(_len > 0){ wout.resize(_len - 1); MultiByteToWideChar(CP_UTF8, 0, out.c_str(), -1, &wout[0], _len); }
    PluginUtil::wstrToRef(s, wout);
}
void SSZ_STDCALL PushRef(PluginUtil*, DynamicRef* ud, lua_State* L) { PushRef(ud, L); }
void SSZ_STDCALL ToRef(PluginUtil*, int32_t i, DynamicRef* ud, lua_State* L) { ToRef(i, ud, L); }

// --- ssz (9) ---
bool            SSZ_STDCALL Run(PluginUtil*, Reference r) { return Run(W(r)); }
CompilerState*  SSZ_STDCALL NewCompiler(PluginUtil*) { return NewCompiler(); }
void            SSZ_STDCALL DeleteCompiler(PluginUtil*, CompilerState* cs) { DeleteCompiler(cs); }
void            SSZ_STDCALL CompilerCompile(PluginUtil*, Reference* err, Reference file, CompilerState* cs) {
    std::wstring out;
    CompilerCompile(&out, W(file), cs);
    PluginUtil::wstrToRef(*err, out);
}
void            SSZ_STDCALL CompilerCompileString(PluginUtil*, Reference* err, Reference name, Reference src, CompilerState* cs) {
    std::wstring out;
    CompilerCompileString(&out, W(name), W(src), cs);
    PluginUtil::wstrToRef(*err, out);
}
bool            SSZ_STDCALL CompilerRun(PluginUtil*, CompilerState* cs) { return CompilerRun(cs); }
void SSZ_STDCALL MemMarkBefore(PluginUtil*, Reference tag) { MemMarkBefore(W(tag)); }
void SSZ_STDCALL MemMarkAfter(PluginUtil*, Reference tag) { MemMarkAfter(W(tag)); }
PluginSSZFuncs* SSZ_STDCALL GetSSZFuncs(PluginUtil*) { return GetSSZFuncs(); }

// --- sdlplugin (62) ---
bool SSZ_STDCALL Init(PluginUtil*, bool m, int32_t h, int32_t w, Reference c) {
    return Init(m, h, w, A_U8(c));
}
bool SSZ_STDCALL GlInit(PluginUtil*, int32_t h, int32_t w, Reference c) {
    return GlInit(h, w, A_U8(c));
}
bool SSZ_STDCALL RendererInit(PluginUtil*, Reference rn, int32_t h, int32_t w, Reference c) {
    return RendererInit(A_U8(rn), h, w, A_U8(c));
}
void SSZ_STDCALL End(PluginUtil*) { End(); }
void SSZ_STDCALL GetRendererInfo(PluginUtil*, Reference o) {
    std::string out;
    GetRendererInfo(&out);
    int _len = MultiByteToWideChar(CP_UTF8, 0, out.c_str(), -1, nullptr, 0);
    std::wstring wout;
    if(_len > 0){ wout.resize(_len - 1); MultiByteToWideChar(CP_UTF8, 0, out.c_str(), -1, &wout[0], _len); }
    PluginUtil::wstrToRef(o, wout);
}
void SSZ_STDCALL EnablePerfMonitor(PluginUtil*, bool e) { EnablePerfMonitor(e); }
void SSZ_STDCALL FullScreenExclusive(PluginUtil*, bool f) { FullScreenExclusive(f); }
bool SSZ_STDCALL FullScreen(PluginUtil*, bool f) { return FullScreen(f); }
void SSZ_STDCALL WindowType(PluginUtil*, int s) { WindowType(s); }
int  SSZ_STDCALL GetWidth(PluginUtil*) { return GetWidth(); }
int  SSZ_STDCALL GetHeight(PluginUtil*) { return GetHeight(); }
void SSZ_STDCALL WindowSize(PluginUtil*, int h, int w) { WindowSize(h, w); }
void SSZ_STDCALL AspectRatio(PluginUtil*, bool a) { AspectRatio(a); }
void SSZ_STDCALL SetOpacity(PluginUtil*, float o) { SetOpacity(o); }
void SSZ_STDCALL TakeScreenShot(PluginUtil*, Reference d) { TakeScreenShot(A_ACP(d)); }
void SSZ_STDCALL Flip(PluginUtil*) { Flip(); }
bool SSZ_STDCALL UpdateGLViewport(PluginUtil*, const SDL_Event& ev) { return UpdateGLViewport(ev); }
bool SSZ_STDCALL PollEvent(PluginUtil*, int8_t* pb) { return PollEvent(pb); }
char16_t SSZ_STDCALL GetLastChar(PluginUtil*) { return GetLastChar(); }
bool SSZ_STDCALL KeyState(PluginUtil*, int32_t k) { return KeyState(k); }
bool SSZ_STDCALL JoystickButtonState(PluginUtil*, int32_t b, int32_t j) { return JoystickButtonState(b, j); }
int32_t SSZ_STDCALL PollInputBitmask(PluginUtil*, int32_t a1, int32_t a2, int32_t a3, int32_t a4, int32_t a5, int32_t a6, int32_t a7, int32_t a8, int32_t a9, int32_t a10, int32_t a11, int32_t a12, int32_t a13, int32_t a14, int32_t a15, int32_t a16, int32_t a17, int32_t a18, int32_t a19, int32_t a20, int32_t a21, int32_t a22, int32_t a23, int32_t a24, int32_t a25, int32_t a26, int32_t a27, int32_t a28, int32_t a29, int32_t a30, int32_t a31) {
    return PollInputBitmask(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31);
}
void SSZ_STDCALL DrawTTF(PluginUtil*, int32_t a, int32_t b, int32_t g, int32_t r, float sy, float sx, int32_t y, int32_t x, Reference txt, int32_t align, Reference fp) {
    DrawTTF(a, b, g, r, sy, sx, y, x, W(txt), align, W(fp));
}
void SSZ_STDCALL SoftFill(PluginUtil*, uint32_t c, SDL_Rect* pr) { SoftFill(c, pr); }
void SSZ_STDCALL Fill(PluginUtil*, uint32_t c, SDL_Rect* pr) { Fill(c, pr); }
intptr_t SSZ_STDCALL IMGLoad(PluginUtil*, Reference fn) { return IMGLoad(A_ACP(fn)); }
void SSZ_STDCALL DecodePNG8(PluginUtil*, FILE* fp, int32_t* h, int32_t* w, Reference out) {
    std::vector<uint8_t> buf;
    DecodePNG8(fp, h, w, &buf);
    out.releaseanddelete();
    out.refnew(buf.size(), 1);
    memcpy(out.atpos(), buf.data(), buf.size());
}
void SSZ_STDCALL BlitSurface(PluginUtil*, SDL_Rect* pr, SDL_Surface* ps) { BlitSurface(pr, ps); }
intptr_t SSZ_STDCALL CreatePaletteSurface(PluginUtil*, int32_t h, int32_t w, SDL_Color* ppl, uint8_t* ppx) {
    return CreatePaletteSurface(h, w, ppl, ppx);
}
void SSZ_STDCALL SetColorKey(PluginUtil*, uint32_t k, SDL_Surface* ps) { SetColorKey(k, ps); }
intptr_t SSZ_STDCALL AllocSurface(PluginUtil*, int32_t h, int32_t w) { return AllocSurface(h, w); }
void SSZ_STDCALL FreeSurface(PluginUtil*, SDL_Surface* ps) { FreeSurface(ps); }
void SSZ_STDCALL Delay(PluginUtil*, uint32_t ms) { Delay(ms); }
uint32_t SSZ_STDCALL GetTicks(PluginUtil*) { return GetTicks(); }
void SSZ_STDCALL CursorShow(PluginUtil*, bool s) { CursorShow(s); }
intptr_t SSZ_STDCALL OpenFont(PluginUtil*, int32_t sz, Reference f) { return OpenFont(sz, A_ACP(f)); }
void SSZ_STDCALL CloseFont(PluginUtil*, TTF_Font* pf) { CloseFont(pf); }
void SSZ_STDCALL RenderFont(PluginUtil*, Reference s, int32_t y, int32_t x, SDL_Color c, TTF_Font* pf) {
    RenderFont(W(s), y, x, c, pf);
}
bool SSZ_STDCALL RenderFontBatch(PluginUtil*, int32_t k, int32_t* j, float g, float h, float i, SDL_Rect* r, int32_t d2, int32_t e, int32_t f, uint32_t* col, float a, float b, uint8_t* d) {
    return RenderFontBatch(k, j, g, h, i, r, d2, e, f, col, a, b, d);
}
bool SSZ_STDCALL SetSndBuf(PluginUtil*, int32_t* b) { return SetSndBuf(b); }
bool SSZ_STDCALL PlayBGM(PluginUtil*, Reference fn, Reference pd) { return PlayBGM(W(fn), W(pd)); }
void SSZ_STDCALL PauseBGM(PluginUtil*, bool p) { PauseBGM(p); }
bool SSZ_STDCALL SendOpenBGM(PluginUtil*, int32_t ch, int32_t r) { return SendOpenBGM(ch, r); }
void SSZ_STDCALL SendCloseBGM(PluginUtil*) { SendCloseBGM(); }
intptr_t SSZ_STDCALL SendWriteBGM(PluginUtil*, Reference buf, intptr_t len) {
    return SendWriteBGM(buf.atpos(), len);
}
void SSZ_STDCALL SetVolume(PluginUtil*, float b, float w, float g) { SetVolume(b, w, g); }
void SSZ_STDCALL FadeInBGM(PluginUtil*, int t) { FadeInBGM(t); }
void SSZ_STDCALL FadeOutBGM(PluginUtil*, int t) { FadeOutBGM(t); }
int SSZ_STDCALL PlayVideo(PluginUtil*, Reference fn, Reference sp, int32_t v, int32_t at) {
    return PlayVideo(W(fn), W(sp), v, at);
}
bool SSZ_STDCALL RenderMugenZoom(PluginUtil*, uint8_t* buf, intptr_t buflen, int32_t rle, float rcy, float rcx, SDL_Rect* dstr, int32_t alpha, uint32_t roto, float rx, float ys, float xb, float xt, SDL_Rect* tile, float ty, float cx, SDL_Rect* sr, uint16_t ck, uint32_t* pp, const uint8_t* pxl, intptr_t pxllen) {
    return RenderMugenZoom(buf, buflen, rle, rcy, rcx, dstr, alpha, roto, rx, ys, xb, xt, tile, ty, cx, sr, ck, pp, pxl, pxllen);
}
bool SSZ_STDCALL RenderMugenShadow(PluginUtil*, uint8_t* buf, intptr_t buflen, int32_t rle, float rcy, float rcx, SDL_Rect* dstr, int32_t alpha, uint32_t roto, float rx, float ys, float xb, float ty, float cx, SDL_Rect* sr, uint32_t ck, const uint8_t* pxl, intptr_t pxllen) {
    return RenderMugenShadow(buf, buflen, rle, rcy, rcx, dstr, alpha, roto, rx, ys, xb, ty, cx, sr, ck, pxl, pxllen);
}
uint32_t SSZ_STDCALL Load8bitTexture(PluginUtil*, int32_t h, int32_t w, uint8_t* px) {
    return Load8bitTexture(h, w, px);
}
uint32_t SSZ_STDCALL LoadPngTexture(PluginUtil*, FILE* fp, int32_t* h, int32_t* w) {
    return LoadPngTexture(fp, h, w);
}
void SSZ_STDCALL DeleteGlTexture(PluginUtil*, uint32_t t) { DeleteGlTexture(t); }
void SSZ_STDCALL GlSwapBuffers(PluginUtil*) { GlSwapBuffers(); }
bool SSZ_STDCALL InitMugenGl(PluginUtil*) { return InitMugenGl(); }
bool SSZ_STDCALL RenderMugenGl(PluginUtil*, float rcy, float rcx, SDL_Rect* dstr, int alpha, float xsc, float ysc, float xs2, float ys2, float xs3, float ys3, SDL_Rect* sr, float x, float y, SDL_Rect* tile, int inter, uint8_t* dec, uint32_t col) {
    return RenderMugenGl(rcy, rcx, dstr, alpha, xsc, ysc, xs2, ys2, xs3, ys3, sr, x, y, tile, inter, dec, col);
}
bool SSZ_STDCALL RenderMugenGlFc(PluginUtil*, float mb, float mg, float mr, float fa, float fb, float fc, float fd, bool fe, float ff, float fg, SDL_Rect* dstr, int a, float x, float y, float w, float h, float xs, float ys, SDL_Rect* sr, float cx, float cy, SDL_Rect* tile, uint32_t col) {
    return RenderMugenGlFc(mb, mg, mr, fa, fb, fc, fd, fe, ff, fg, dstr, a, x, y, w, h, xs, ys, sr, cx, cy, tile, col);
}
bool SSZ_STDCALL RenderMugenGlFcS(PluginUtil*, uint32_t col, float cx, float cy, SDL_Rect* dstr, int a, float x, float y, float w, float h, float xs, float ys, SDL_Rect* sr, float xc, float yc, SDL_Rect* tile, uint32_t col2) {
    return RenderMugenGlFcS(col, cx, cy, dstr, a, x, y, w, h, xs, ys, sr, xc, yc, tile, col2);
}
void SSZ_STDCALL MugenFillGl(PluginUtil*, int32_t a, uint32_t c, SDL_Rect r) { MugenFillGl(a, c, r); }
bool SSZ_STDCALL BindGlContext(PluginUtil*) { return BindGlContext(); }
bool SSZ_STDCALL UnbindGlContext(PluginUtil*) { return UnbindGlContext(); }
} // extern "C"
