// ============================================================================
// sdlplugin.cpp — native (C++) implementation of the SSZ `sdlplugin` library.
//
// Converted from ssz_script/lib/alpha/sdlplugin.ssz.  The SSZ file keeps the
// type vocabulary (|EventType, |SDLKey, |K, &Event, &Rect, &GlTexture, ...)
// and delegates its module-level functions here:
//
//     lib sdlp = <sdlplugin>;
//     public void flip() { .sdlp.flip(); }
//
// Every function is the SAME C++ implementation the static <sdlplugin>
// plugin already exports (main/sdlplugin/sdlplugin.cpp, registered via
// sdlplugin_plugin.hpp) — the native registry and the static plugin registry
// are separate, so the same function pointers are reachable as both
// `plugin ... = <sdlplugin>;` and `lib sdlp = <sdlplugin>;`.
//
// The signature strings are the SSZ views of the plugin declarations they
// replace, including struct/enum types (`&Event=`, `&.Rect=`, `&GlTexture`,
// `|SDLKey`, `&.f.File`) — resolved by the native registry's type resolver
// when the module is imported (the importing script must already have
// declared those types).
//
// ABI: plugin convention — args arrive reversed; 32-bit args in the low 32
// bits of an 8-byte slot; `type=` out-params arrive as pointers; `&Struct`
// by-value params pass the struct memory; `^/x` strings arrive as Reference.
// ============================================================================

#include "sszdef.h"
#include "arrayandref.hpp"   // full Reference definition (sdlplugin_plugin.hpp assumes it)
#include "native_lib.hpp"

// Declares every function below with the plugin ABI (extern "C").
#include "sdlplugin/sdlplugin_plugin.hpp"

// ---------------------------------------------------------------------------
// Registration — re-exports the static plugin's function pointers under the
// native library name `sdlplugin`, with the SSZ signature strings from the
// original plugin declarations in alpha/sdlplugin.ssz.
// ---------------------------------------------------------------------------

extern "C" bool sdlplugin_lib_register()
{
	static const NativeLib::NativeFunction funcs[] = {
		// Initialisation / window
		{ "End",                 "void ()",                                     (void*)End                 },
		{ "init",                "bool (^/char, int, int, ^/char)",             (void*)RendererInit        },
		{ "getWidth",            "int ()",                                      (void*)GetWidth            },
		{ "getHeight",           "int ()",                                      (void*)GetHeight           },
		{ "windowSize",          "void (int, int)",                             (void*)WindowSize          },
		{ "fullScreenMode",      "void (bool)",                                 (void*)FullScreenExclusive },
		{ "fullScreen",          "bool (bool)",                                 (void*)FullScreen          },
		{ "setWindowType",       "void (int)",                                  (void*)WindowType          },
		{ "keepAspectRatio",     "void (bool)",                                 (void*)AspectRatio         },
		{ "takeScreenShot",      "void (^/char)",                               (void*)TakeScreenShot      },
		{ "showCursor",          "void (bool)",                                 (void*)CursorShow          },
		{ "enablePerfMonitor",   "void (bool)",                                 (void*)EnablePerfMonitor   },
		{ "getRendererInfo",     "void ()",                                     (void*)GetRendererInfo     },

		// Events / input
		{ "pollEvent",           "bool (&Event=)",                              (void*)PollEvent           },
		{ "updateGLViewport",    "bool (&Event=)",                              (void*)UpdateGLViewport    },
		{ "getLastChar",         "char ()",                                     (void*)GetLastChar         },
		{ "keyState",            "bool (|SDLKey)",                              (void*)KeyState            },
		{ "joystickButtonState", "bool (int, int)",                             (void*)JoystickButtonState },
		{ "pollInputBitmask",    "int (int, int, int, int, int, int, int, int, int, int, int, int, int, int, int, int, int, int, int, int, int, int, int, int, int, int, int, int, int, int, int)", (void*)PollInputBitmask },
		{ "delay",               "void (uint)",                                 (void*)Delay               },
		{ "getTicks",            "uint ()",                                     (void*)GetTicks            },
		{ "setSndBuf",           "bool (int=)",                                 (void*)SetSndBuf           },

		// Rendering
		{ "flip",                "void ()",                                     (void*)Flip                },
		{ "fill",                "void (&.Rect=, uint)",                        (void*)Fill                },
		{ "softFill",            "void (&.Rect=, uint)",                        (void*)SoftFill            },
		{ "renderMugenZoom",     "bool (^/ubyte, uint=, short, &.Rect=, float, float, &.Rect=, float, float, float, float, uint, int, &.Rect=, float, float, int, %byte=)", (void*)RenderMugenZoom },
		{ "renderMugenShadow",   "bool (^/ubyte, uint, &.Rect=, float, float, float, float, float, uint, int, &.Rect=, float, float, int, %byte=)", (void*)RenderMugenShadow },
		{ "renderFontBatch",     "bool (ubyte=, float, float, uint=, int, int, int, &.Rect=, float, float, float, int=, int)", (void*)RenderFontBatch },
		{ "mugenFillGl",         "void (&Rect, uint, int)",                     (void*)MugenFillGl         },
		{ "renderMugenGl",       "bool (&GlTexture, uint=, int, &Rect=, float, float, &Rect=, float, float, float, float, float, float, int, &Rect=, float, float)", (void*)RenderMugenGl },
		{ "renderMugenGlFc",     "bool (&GlTexture, &Rect=, float, float, &Rect=, float, float, float, float, float, float, int, &Rect=, float, float, bool, float, float, float, float, float, float, float)", (void*)RenderMugenGlFc },
		{ "renderMugenGlFcS",    "bool (&GlTexture, &Rect=, float, float, &Rect=, float, float, float, float, float, float, int, &Rect=, float, float, uint)", (void*)RenderMugenGlFcS },
		{ "glSwapBuffers",       "void ()",                                     (void*)GlSwapBuffers       },
		{ "bindGlContext",       "bool ()",                                     (void*)BindGlContext       },
		{ "unbindGlContext",     "bool ()",                                     (void*)UnbindGlContext     },

		// Fonts / textures (surface/font/texture helpers)
		{ "imgLoad",             "index (^/char)",                              (void*)IMGLoad             },
		{ "freeSurface",         "void (index)",                                (void*)FreeSurface         },
		{ "allocSurface",        "index (int, int)",                            (void*)AllocSurface        },
		{ "blitSurface",         "void (index, &.Rect=)",                       (void*)BlitSurface         },
		{ "createPaletteSurface","index (ubyte=, uint=, int, int)",             (void*)CreatePaletteSurface},
		{ "setColorKey",         "index (index, int)",                          (void*)SetColorKey         },
		{ "openFont",            "index (^/char, int)",                         (void*)OpenFont            },
		{ "closeFont",           "void (index)",                                (void*)CloseFont           },
		{ "renderFont",          "void (index, uint, int, int, ^/char)",        (void*)RenderFont          },
		{ "deleteGlTexture",     "void (uint)",                                 (void*)DeleteGlTexture     },
		{ "load8bitTexture",     "uint (ubyte=, int, int)",                     (void*)Load8bitTexture     },
		{ "loadPngTexture",      "uint (int=, int=, &.f.File)",                 (void*)LoadPngTexture      },
		{ "decodePNG8",          "void (^ubyte=, int=, int=, &.f.File)",        (void*)DecodePNG8          },

		// Audio / video
		{ "playVideo",           "int (int, int, ^/char, ^/char)",              (void*)PlayVideo           },
		{ "playBGM",             "bool (^/char, ^/char)",                       (void*)PlayBGM             },
		{ "pauseBGM",            "void (bool)",                                 (void*)PauseBGM            },
		{ "sendOpenBGM",         "bool (int, int)",                             (void*)SendOpenBGM         },
		{ "sendCloseBGM",        "void ()",                                     (void*)SendCloseBGM        },
		{ "sendWriteBGM",        "index (^/short)",                             (void*)SendWriteBGM        },
		{ "setVolume",           "void (float, float, float)",                  (void*)SetVolume           },
		{ "fadeInBGM",           "void (int)",                                  (void*)FadeInBGM           },
		{ "fadeOutBGM",          "void (int)",                                  (void*)FadeOutBGM          },
		{ "setOpacity",          "void (float)",                                (void*)SetOpacity          },
		{ "drawTTF",             "void (^/char, int, ^/char, int, int, float, float, int, int, int, int)", (void*)DrawTTF },
	};

	NativeLib::NativeLibrary lib;
	lib.name = "sdlplugin";
	for(size_t i = 0; i < sizeof(funcs) / sizeof(funcs[0]); i++){
		lib.functions.push_back(funcs[i]);
	}
	return NativeLib::RegisterLibrary(lib);
}
