// ============================================================================
// video.cpp — native (C++) implementation of SSZ `video` module.
//
// Transpiled from ssz_script/ssz/video.ssz via tools/ssz_to_cpp.py.
// The &Video struct stays in the .ssz wrapper (type definition consumed by
// other scripts); its method bodies delegate here through the native lib.
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
// Static plugin declarations (from main/file/file_plugin.hpp,
// main/sdlplugin/sdlplugin_plugin.hpp)
// ---------------------------------------------------------------------------

extern "C" {
    intptr_t SSZ_STDCALL Open(PluginUtil*, Reference mode, Reference filename);
    int      SSZ_STDCALL PlayVideo(PluginUtil*, Reference filename,
                                   Reference capturepath,
                                   int32_t audiotrack, int32_t volume);
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Create a Reference from a narrow string literal (for plugin calls).
static Reference MakeRef(const char* s)
{
#ifdef _WIN32
    // Convert narrow to wide
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
// &Video::clear()  —  initialize default values (no struct fields to write)
// ---------------------------------------------------------------------------

static void SSZ_STDCALL VideoClear(PluginUtil* pu)
{
    // clear() sets local variables — no out-params to the struct.
    // The method body is:
    //   ^/char file = "";
    //   ^/char capturepath = "";
    //   int volume = 100;
    //   int audiotrack = 1;
    //   int subtitletrack = 0;
    // These are all locals; nothing to return or write back.
}

// ---------------------------------------------------------------------------
// &Video::play(^/char file, ^/char capturepath, int volume, int audiotrack)
//
// SSZ declaration order:  file, capturepath, volume, audiotrack
// Reversed C++ ABI:      audiotrack, volume, capturepath, file, fileName=
// ---------------------------------------------------------------------------

static void SSZ_STDCALL VideoPlay(
    PluginUtil* pu,
    int32_t audiotrack, int32_t volume,
    Reference capturepath, Reference file,
    Reference* fileName)
{
    // f.open(file, "rb")
    Reference modeRef = MakeRef("rb");
    intptr_t fh = Open(pu, modeRef, file);

    if (fh != 0) {
        // `fileName = file;
        if (fileName) {
            *fileName = file;
        }

        // .sdl.playVideo(audiotrack, volume, capturepath, `fileName)
        Reference fnRef = fileName ? *fileName : file;
        PlayVideo(pu, fnRef, capturepath, audiotrack, volume);

        // .videoActive = true  — module-level var, stays in SSZ wrapper
    }
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

extern "C" bool video_native_lib_register()
{
    static const NativeLib::NativeFunction funcs[] = {
        { "Video_clear", "void ()",
          (void*)VideoClear },
        { "Video_play",  "void (^/char=, ^/char, ^/char, int, int)",
          (void*)VideoPlay },
    };

    NativeLib::NativeLibrary lib;
    lib.name = "video_native";
    for (size_t i = 0; i < sizeof(funcs) / sizeof(funcs[0]); i++)
        lib.functions.push_back(funcs[i]);
    return NativeLib::RegisterLibrary(lib);
}
