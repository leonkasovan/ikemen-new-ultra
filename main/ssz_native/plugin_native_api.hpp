#pragma once

// plugin_native_api.hpp — Single source of truth for native C++ plugin function
// declarations that are needed across multiple TUs.
//
// Currently covers: file, math, time, alert, thread.
// Plugin-specific declarations (regex, socket, sound, ogg, mesdialog, lua, sdl, shell)
// live in bridge.cpp since they are only referenced from bridge wrappers there.
// If a future ssz_native module needs them, move the declarations here.
// TODO: Remaining plugins not yet in this header:
//   - regex, socket, sound, ogg (Phase 2 of TODO_SSZ_CONVERSION.md)
//   - mesdialog, lua (Phase 2 — deferred until Lua boundary is understood)
//   - sdl (Phase 2 — late conversion, many SDL object lifetimes)
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
