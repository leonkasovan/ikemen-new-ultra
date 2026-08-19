// ============================================================================
// sound_game.cpp — native (C++) implementation of parts of the game script
// ssz_script/ssz/sound.ssz.
//
// Converts &Bgm methods (play/clear/write) and &Sound.setVol to native code,
// eliminating the SSZ→plugin bridge overhead for BGM playback and volume
// clamping.
//
// The audio mixing (&Sound.mix and its helpers), file parsing (&Snd.loadFile),
// and all struct definitions stay in the .ssz wrapper — their method bodies
// use SSZ-specific types (^&.Wave, &.tbl.IntTable, delegates) that can't be
// expressed in native function signatures.
//
// ABI: plugin convention — args reversed; out-params as pointers;
// strings as Reference.
// ============================================================================

#include <cstdint>
#include <string>

#include "sszdef.h"
#include "native_lib.hpp"
#include "arrayandref.hpp"
#include "pluginutil.hpp"

struct PluginUtil;

// ---------------------------------------------------------------------------
// Static plugin declarations (from main/sdlplugin/sdlplugin_plugin.hpp)
// ---------------------------------------------------------------------------

extern "C" {
    bool SSZ_STDCALL PlayBGM(PluginUtil*, Reference fn, Reference pldir);
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Create a Reference from a wide string literal.
static Reference MakeRefW(const wchar_t* s)
{
    Reference ref;
    PluginUtil::wstrToRef(ref, s);
    return ref;
}

// Create a Reference from a narrow string literal.
static Reference MakeRef(const char* s)
{
#ifdef _WIN32
    int len = MultiByteToWideChar(CP_UTF8, 0, s, -1, nullptr, 0);
    std::wstring ws(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s, -1, &ws[0], len);
    ws.resize(len - 1);  // remove null terminator from wstring
    Reference ref;
    PluginUtil::wstrToRef(ref, ws);
    return ref;
#else
    Reference ref;
    std::wstring ws(s, s + strlen(s));
    PluginUtil::wstrToRef(ref, ws);
    return ref;
#endif
}

// ---------------------------------------------------------------------------
// &Bgm::play(^/char file)  —  clear then play BGM
//
// SSZ declaration order:  file
// Reversed C++ ABI:      file, fileName=
//
// Original SSZ body:
//   `clear();
//    if(.sdl.playBGM("", file)) `fileName = file;
// ---------------------------------------------------------------------------

static void SSZ_STDCALL BgmPlay(
    PluginUtil* pu,
    Reference file,
    Reference* fileName)
{
    Reference emptyRef = MakeRefW(L"");

    // clear(): playBGM("", ""), fileName = ""
    PlayBGM(pu, emptyRef, emptyRef);
    if (fileName) *fileName = emptyRef;

    // playBGM("", file)
    if (PlayBGM(pu, emptyRef, file)) {
        if (fileName) *fileName = file;
    }
}

// ---------------------------------------------------------------------------
// &Bgm::clear()  —  stop BGM and reset fileName
//
// SSZ declaration order:  (none — only out-param)
// Reversed C++ ABI:      fileName=
//
// Original SSZ body:
//   `fileName = "";
//    .sdl.playBGM("", "");
// ---------------------------------------------------------------------------

static void SSZ_STDCALL BgmClear(
    PluginUtil* pu,
    Reference* fileName)
{
    Reference emptyRef = MakeRefW(L"");

    if (fileName) *fileName = emptyRef;
    PlayBGM(pu, emptyRef, emptyRef);
}

// ---------------------------------------------------------------------------
// &Bgm::write()  —  no-op (BGM playback handled by SDL_mixer)
//
// Original SSZ body:
//   // BGM playback is handled by SDL_mixer – nothing to pump.
// ---------------------------------------------------------------------------

static void SSZ_STDCALL BgmWrite(PluginUtil* pu)
{
    // No-op.
}

// ---------------------------------------------------------------------------
// &Sound::setVol(int v)  —  clamp volume to [0, 512]
//
// SSZ declaration order:  v
// Reversed C++ ABI:      v, volume=
//
// Original SSZ body:
//   branch{
//   cond v < 0: `volume = 0;
//   cond v > 512: `volume = 512;
//   else: `volume = v;
//   }
//
// Note: `volume` is `short` in SSZ (16-bit), but the frame slot is 8 bytes.
// We write through int32_t* to be safe — the JIT reads back only the low
// 16 bits for a short value.
// ---------------------------------------------------------------------------

static void SSZ_STDCALL SoundSetVol(
    PluginUtil* pu,
    int32_t v,
    int32_t* volume)
{
    if (v < 0) *volume = 0;
    else if (v > 512) *volume = 512;
    else *volume = v;
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

extern "C" bool sound_game_lib_register()
{
    static const NativeLib::NativeFunction funcs[] = {
        { "Bgm_play",     "void (^/char=, ^/char)",   (void*)BgmPlay     },
        { "Bgm_clear",    "void (^/char=)",            (void*)BgmClear    },
        { "Bgm_write",    "void ()",                   (void*)BgmWrite    },
        { "Sound_setVol", "void (short=, int)",         (void*)SoundSetVol },
    };

    NativeLib::NativeLibrary lib;
    lib.name = "sound_game";
    for (size_t i = 0; i < sizeof(funcs) / sizeof(funcs[0]); i++)
        lib.functions.push_back(funcs[i]);
    return NativeLib::RegisterLibrary(lib);
}
