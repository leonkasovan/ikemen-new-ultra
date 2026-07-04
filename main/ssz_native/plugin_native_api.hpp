#pragma once

// plugin_native_api.hpp — Single source of truth for native C++ plugin function
// declarations that are needed across multiple TUs.
//
// Currently covers: file, math, time, alert, thread, sdlplugin.
// Plugin-specific declarations (regex, socket, sound, ogg, mesdialog, lua, shell)
// live in bridge.cpp since they are only referenced from bridge wrappers there.
// If a future ssz_native module needs them, move the declarations here.
// TODO: Remaining plugins not yet in this header:
//   - regex, socket, sound, ogg (Phase 2 of TODO_SSZ_CONVERSION.md)
//   - mesdialog, lua (Phase 2 — deferred until Lua boundary is understood)
//   - shell (Phase 3 — simple, but bridge.cpp-only currently)
//   - SSZCALLBACK typedef (lives in bridge.cpp; migrate here when lua declarations move)
//
// All functions here have C++ linkage (no extern "C") and use native types
// (std::wstring, std::vector, primitives, raw pointers). They are defined
// in their respective main/*.cpp files.

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#ifndef SSZ_STDCALL
#define SSZ_STDCALL __stdcall
#endif

// Forward declarations for SDL types
struct SDL_Rect;
struct SDL_Surface;
struct SDL_Color;
union SDL_Event;
typedef struct _TTF_Font TTF_Font;
struct Reference;

// ---- File plugin (main/file/file.cpp) ----

intptr_t SSZ_STDCALL Open(const std::wstring& md, const std::wstring& fn);
void     SSZ_STDCALL FileClose(FILE* pFile);
bool     SSZ_STDCALL Read(intptr_t size, void* p, FILE* pFile);
intptr_t SSZ_STDCALL ReadAry(intptr_t size, void* data, intptr_t bytes, FILE* pFile);
bool     SSZ_STDCALL Write(intptr_t size, const void* p, FILE* pFile);
intptr_t SSZ_STDCALL WriteAry(intptr_t size, const void* data, intptr_t bytes, FILE* pFile);
bool     SSZ_STDCALL Seek(int32_t origin, int64_t offset, FILE* pFile);
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

// ---- Math plugin (main/math/math.cpp) ----

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

// ---- Time plugin (main/time/time.cpp) ----

uint32_t SSZ_STDCALL TickCount();
int64_t  SSZ_STDCALL UnixTime();

// ---- Alert plugin (main/alert/alert.cpp) ----

void SSZ_STDCALL Alert(const std::wstring& title, const std::wstring& mes);

// ---- Thread plugin (main/thread/thread.cpp) ----

void SSZ_STDCALL ThreadDelay(uint32_t ui);

// ---- SDL plugin (main/sdlplugin/sdlplugin.cpp) ----

void       SSZ_STDCALL Flip();
void       SSZ_STDCALL Fill(uint32_t color, SDL_Rect* prect);
void       SSZ_STDCALL SoftFill(uint32_t color, SDL_Rect* prect);
bool       SSZ_STDCALL RenderMugenZoom(Reference* pluginbuf, int32_t rle, float rcy, float rcx, SDL_Rect* pdstr, int32_t alpha, uint32_t roto, float rasterxadd, float yscl, float xbotscl, float xtopscl, SDL_Rect* tile, float ty, float cx, SDL_Rect* psrcr, uint16_t ckey, uint32_t* ppal, Reference img);
bool       SSZ_STDCALL RenderFontBatch(int32_t count, int32_t* glyphData, float spacing, float yscl, float xscl, SDL_Rect* window, int32_t alpha, int32_t glyphH, int32_t atlasStride, uint32_t* ppal, float baseY, float baseX, uint8_t* atlasPixels);
bool       SSZ_STDCALL RenderMugenShadow(Reference* pluginbuf, int32_t rle, float rcy, float rcx, SDL_Rect* pdstr, int32_t alpha, uint32_t roto, float vscl, float yscl, float xscl, float ty, float cx, SDL_Rect* psrcr, uint32_t color, Reference img);
char16_t   SSZ_STDCALL GetLastChar();
void       SSZ_STDCALL DecodePNG8(FILE* fp, int32_t* h, int32_t* w, std::vector<uint8_t>& out);
bool       SSZ_STDCALL KeyState(int32_t key);
bool       SSZ_STDCALL JoystickButtonState(int32_t btn, int32_t joy);
bool       SSZ_STDCALL PollEvent(int8_t* pb);
uint32_t   SSZ_STDCALL GetTicks();
void       SSZ_STDCALL Delay(uint32_t ms);
int32_t    SSZ_STDCALL PollInputBitmask(int32_t jn, int32_t u, int32_t d, int32_t l, int32_t r, int32_t a, int32_t b, int32_t c, int32_t x, int32_t y, int32_t z, int32_t q, int32_t w, int32_t e, int32_t s, int32_t jn2, int32_t u2, int32_t d2, int32_t l2, int32_t r2, int32_t a2, int32_t b2, int32_t c2, int32_t x2, int32_t y2, int32_t z2, int32_t q2, int32_t w2, int32_t e2, int32_t s2, int32_t sec);
bool       SSZ_STDCALL SetSndBuf(int32_t* buf);
int        SSZ_STDCALL PlayVideo(const std::wstring& fn, const std::wstring& screenshotPath, int volume, int audioTrack);
bool       SSZ_STDCALL PlayBGM(const std::wstring& fn, const std::wstring& pldir);
void       SSZ_STDCALL PauseBGM(bool pause);
bool       SSZ_STDCALL SendOpenBGM(int32_t channels, int32_t rate);
void       SSZ_STDCALL SendCloseBGM();
intptr_t   SSZ_STDCALL SendWriteBGM();
void       SSZ_STDCALL FadeInBGM(int time);
void       SSZ_STDCALL FadeOutBGM(int time);
void       SSZ_STDCALL SetVolume(float bv, float wv, float gv);
void       SSZ_STDCALL SetOpacity(float wo);
bool       SSZ_STDCALL Init(bool mugen, int32_t h, int32_t w, const std::wstring& cap);
bool       SSZ_STDCALL GlInit(int32_t h, int32_t w, const std::wstring& cap);
bool       SSZ_STDCALL RendererInit(const std::wstring& rendererName, int32_t h, int32_t w, const std::wstring& cap);
int        SSZ_STDCALL GetWidth();
int        SSZ_STDCALL GetHeight();
void       SSZ_STDCALL WindowSize(int height, int width);
void       SSZ_STDCALL FullScreenExclusive(bool fsr);
bool       SSZ_STDCALL FullScreen(bool fs);
void       SSZ_STDCALL WindowType(int state);
void       SSZ_STDCALL AspectRatio(bool aspect);
void       SSZ_STDCALL TakeScreenShot(const std::wstring& dir);
void       SSZ_STDCALL CursorShow(bool show);
bool       SSZ_STDCALL BindGlContext();
bool       SSZ_STDCALL UnbindGlContext();
void       SSZ_STDCALL EnablePerfMonitor(bool enable);
void       SSZ_STDCALL GetRendererInfo();
intptr_t   SSZ_STDCALL AllocSurface(int32_t h, int32_t w);
void       SSZ_STDCALL FreeSurface(SDL_Surface* ps);
intptr_t   SSZ_STDCALL IMGLoad(const std::wstring& fn);
void       SSZ_STDCALL BlitSurface(SDL_Rect* prect, SDL_Surface* psrcs);
intptr_t   SSZ_STDCALL CreatePaletteSurface(int32_t h, int32_t w, SDL_Color* ppl, uint8_t* ppx);
void       SSZ_STDCALL SetColorKey(uint32_t key, SDL_Surface* psur);
intptr_t   SSZ_STDCALL OpenFont(int32_t size, const std::wstring& font);
void       SSZ_STDCALL CloseFont(TTF_Font* pf);
void       SSZ_STDCALL RenderFont(const std::wstring& str, int32_t y, int32_t x, SDL_Color c, TTF_Font* pf);
uint32_t   SSZ_STDCALL Load8bitTexture(int32_t h, int32_t w, uint8_t* ppxl);
uint32_t   SSZ_STDCALL LoadPngTexture(FILE* fp, int32_t* h, int32_t* w);
void       SSZ_STDCALL DeleteGlTexture(uint32_t texid);
