#pragma once
//
// sdlplugin_static.hpp
//
// Statically registers every function exported by sdlplugin.cpp
// so that the SSZ runtime resolves them without loading sdlplugin.dll.
//
// HOW TO USE
// ----------
// 1.  #include this header in the translation unit that owns main()
//     (or wherever the SSZ compiler is initialised).
//
// 2.  Call  sdlplugin_static_register()  BEFORE the SSZ compiler runs.
//
//     #include "sdlplugin_static.hpp"
//
//     int main() {
//         if (!sdlplugin_static_register()) {
//             printf("Failed to register sdlplugin functions.\n");
//             return 1;
//         }
//         // ... start SSZ compiler / runtime ...
//     }
//
// 3.  The SSZ scripts continue to use:
//         plugin void Flip() = "dll/sdlplugin.dll";
//     but NO sdlplugin.dll file is needed — the function pointer is
//     resolved from the static registry.
//
// PREREQUISITES
// -------------
//   - Link sdlplugin.cpp (and its dependencies) into the same executable.
//   - #include "static_plugin_registry.hpp" (pulled in automatically).
//

#include "static_plugin_registry.hpp"
#include "SDL.h"
#include "SDL_ttf.h"

// -----------------------------------------------------------------------
// Forward-declare types needed in function signatures.
// (sszdef.h, typeid.h, and arrayandref.hpp are assumed to be
//  already included by the caller before this header is included.)
// -----------------------------------------------------------------------

struct PluginUtil;   // forward (from pluginutil.hpp)
struct Reference;    // forward (from arrayandref.hpp)

extern "C"
{
	// Initialisation / shutdown
	bool      SSZ_STDCALL Init               (PluginUtil*, bool, int32_t, int32_t, Reference);
	bool      SSZ_STDCALL GlInit             (PluginUtil*, int32_t, int32_t, Reference);
	bool      SSZ_STDCALL RendererInit       (PluginUtil*, int32_t, int32_t, Reference, Reference);
	void      SSZ_STDCALL End                (PluginUtil*);

	// Renderer info / performance
	void      SSZ_STDCALL GetRendererInfo    (PluginUtil*, Reference*);
	void      SSZ_STDCALL EnablePerfMonitor  (PluginUtil*, bool);

	// Window / display
	void      SSZ_STDCALL FullScreenExclusive(PluginUtil*, bool);
	bool      SSZ_STDCALL FullScreen         (PluginUtil*, bool);
	void      SSZ_STDCALL WindowType         (PluginUtil*, int);
	int       SSZ_STDCALL GetWidth           (PluginUtil*);
	int       SSZ_STDCALL GetHeight          (PluginUtil*);
	void      SSZ_STDCALL WindowSize         (PluginUtil*, int, int);
	void      SSZ_STDCALL AspectRatio        (PluginUtil*, bool);
	void      SSZ_STDCALL SetOpacity         (PluginUtil*, float);
	void      SSZ_STDCALL TakeScreenShot     (PluginUtil*, Reference);
	void      SSZ_STDCALL Flip               (PluginUtil*);
	bool      SSZ_STDCALL UpdateGLViewport   (PluginUtil*, const SDL_Event&);

	// Input
	bool      SSZ_STDCALL PollEvent          (PluginUtil*, int8_t*);
	char16_t  SSZ_STDCALL GetLastChar        (PluginUtil*);
	bool      SSZ_STDCALL KeyState           (PluginUtil*, int32_t);
	bool      SSZ_STDCALL JoystickButtonState(PluginUtil*, int32_t, int32_t);
	int32_t   SSZ_STDCALL PollInputBitmask   (PluginUtil*,
	              int32_t, int32_t, int32_t, int32_t, int32_t,
	              int32_t, int32_t, int32_t, int32_t, int32_t,
	              int32_t, int32_t, int32_t, int32_t, int32_t,
	              int32_t, int32_t, int32_t, int32_t, int32_t,
	              int32_t, int32_t, int32_t, int32_t, int32_t,
	              int32_t, int32_t, int32_t, int32_t, int32_t,
	              int32_t);

	// 2-D rendering (SDL)
	void      SSZ_STDCALL DrawTTF            (PluginUtil*, int32_t, int32_t, int32_t, int32_t,
	                                          float, float, int32_t, int32_t,
	                                          Reference, int32_t, Reference);
	void      SSZ_STDCALL SoftFill           (PluginUtil*, uint32_t, SDL_Rect*);
	void      SSZ_STDCALL Fill               (PluginUtil*, uint32_t, SDL_Rect*);
	intptr_t  SSZ_STDCALL IMGLoad            (PluginUtil*, Reference);
	void      SSZ_STDCALL DecodePNG8         (PluginUtil*, FILE*, int32_t*, int32_t*, Reference*);
	void      SSZ_STDCALL BlitSurface        (PluginUtil*, SDL_Rect*, SDL_Surface*);
	intptr_t  SSZ_STDCALL CreatePaletteSurface(PluginUtil*, int32_t, int32_t, SDL_Color*, uint8_t*);
	void      SSZ_STDCALL SetColorKey        (PluginUtil*, uint32_t, SDL_Surface*);
	intptr_t  SSZ_STDCALL AllocSurface       (PluginUtil*, int32_t, int32_t);
	void      SSZ_STDCALL FreeSurface        (PluginUtil*, SDL_Surface*);

	// Timing / cursor
	void      SSZ_STDCALL Delay              (PluginUtil*, uint32_t);
	uint32_t  SSZ_STDCALL GetTicks           (PluginUtil*);
	void      SSZ_STDCALL CursorShow         (PluginUtil*, bool);

	// Font / text
	intptr_t  SSZ_STDCALL OpenFont           (PluginUtil*, int32_t, Reference);
	void      SSZ_STDCALL CloseFont          (PluginUtil*, TTF_Font*);
	void      SSZ_STDCALL RenderFont         (PluginUtil*, Reference, int32_t, int32_t, SDL_Color, TTF_Font*);
	bool      SSZ_STDCALL RenderFontBatch    (PluginUtil*, uint8_t*, float, float, uint32_t, int32_t, int32_t, int32_t, SDL_Rect*, float, float, float, int32_t*, int32_t);

	// Audio
	bool      SSZ_STDCALL SetSndBuf          (PluginUtil*, int32_t*);
	bool      SSZ_STDCALL PlayBGM            (PluginUtil*, Reference, Reference);
	void      SSZ_STDCALL PauseBGM           (PluginUtil*, bool);
	bool      SSZ_STDCALL SendOpenBGM        (PluginUtil*, int32_t, int32_t);
	void      SSZ_STDCALL SendCloseBGM       (PluginUtil*);
	intptr_t  SSZ_STDCALL SendWriteBGM       (PluginUtil*, Reference);
	void      SSZ_STDCALL SetVolume          (PluginUtil*, float, float, float);
	void      SSZ_STDCALL FadeInBGM          (PluginUtil*, int);
	void      SSZ_STDCALL FadeOutBGM         (PluginUtil*, int);

	// Video
	int       SSZ_STDCALL PlayVideo          (PluginUtil*, Reference, Reference, int32_t, int32_t);

	// Mugen-style software rendering
	bool      SSZ_STDCALL RenderMugenZoom    (PluginUtil*, Reference*, int32_t,
	                                          float, float, SDL_Rect*, int32_t, uint32_t,
	                                          float, float, float, float,
	                                          SDL_Rect*, float, float, SDL_Rect*,
	                                          uint16_t, uint32_t*, Reference);
	bool      SSZ_STDCALL RenderMugenShadow  (PluginUtil*, Reference*, int32_t,
	                                          float, float, SDL_Rect*, int32_t, uint32_t,
	                                          float, float, float,
	                                          float, float, SDL_Rect*, uint32_t, Reference);

	// OpenGL rendering
	uint32_t  SSZ_STDCALL Load8bitTexture    (PluginUtil*, int32_t, int32_t, uint8_t*);
	uint32_t  SSZ_STDCALL LoadPngTexture     (PluginUtil*, FILE*, int32_t*, int32_t*);
	void      SSZ_STDCALL DeleteGlTexture    (PluginUtil*, uint32_t);
	void      SSZ_STDCALL GlSwapBuffers      (PluginUtil*);
	bool      SSZ_STDCALL InitMugenGl        (PluginUtil*);
	bool      SSZ_STDCALL RenderMugenGl      (PluginUtil*, float, float, SDL_Rect*, int,
	                                          float, float, float, float, float, float,
	                                          SDL_Rect*, float, float, SDL_Rect*,
	                                          int, uint8_t*, uint32_t);
	bool      SSZ_STDCALL RenderMugenGlFc    (PluginUtil*, float, float, float,
	                                          float, float, float, float, bool,
	                                          float, float, SDL_Rect*, int,
	                                          float, float, float, float, float, float,
	                                          SDL_Rect*, float, float, SDL_Rect*, uint32_t);
	bool      SSZ_STDCALL RenderMugenGlFcS   (PluginUtil*, uint32_t, float, float,
	                                          SDL_Rect*, int, float, float, float, float,
	                                          float, float, SDL_Rect*, float, float,
	                                          SDL_Rect*, uint32_t);
	void      SSZ_STDCALL MugenFillGl        (PluginUtil*, int32_t, uint32_t, SDL_Rect);
	bool      SSZ_STDCALL BindGlContext      (PluginUtil*);
	bool      SSZ_STDCALL UnbindGlContext    (PluginUtil*);
}

// -----------------------------------------------------------------------
// Build the mapping table and register it.
// -----------------------------------------------------------------------

/// Call once before the SSZ compiler starts.
/// Returns true on success.
inline bool sdlplugin_static_register()
{
	static const SSZ_FunctionEntry sdlplugin_mapping[] =
	{
		{ "Init",                (void*)Init                },
		{ "GlInit",              (void*)GlInit              },
		{ "RendererInit",        (void*)RendererInit        },
		{ "End",                 (void*)End                 },
		{ "GetRendererInfo",     (void*)GetRendererInfo     },
		{ "EnablePerfMonitor",   (void*)EnablePerfMonitor   },
		{ "FullScreenExclusive", (void*)FullScreenExclusive },
		{ "FullScreen",          (void*)FullScreen          },
		{ "WindowType",          (void*)WindowType          },
		{ "GetWidth",            (void*)GetWidth            },
		{ "GetHeight",           (void*)GetHeight           },
		{ "WindowSize",          (void*)WindowSize          },
		{ "AspectRatio",         (void*)AspectRatio         },
		{ "SetOpacity",          (void*)SetOpacity          },
		{ "TakeScreenShot",      (void*)TakeScreenShot      },
		{ "Flip",                (void*)Flip                },
		{ "UpdateGLViewport",    (void*)UpdateGLViewport    },
		{ "PollEvent",           (void*)PollEvent           },
		{ "GetLastChar",         (void*)GetLastChar         },
		{ "KeyState",            (void*)KeyState            },
		{ "JoystickButtonState", (void*)JoystickButtonState },
		{ "PollInputBitmask",   (void*)PollInputBitmask   },
		{ "DrawTTF",             (void*)DrawTTF             },
		{ "SoftFill",            (void*)SoftFill            },
		{ "Fill",                (void*)Fill                },
		{ "IMGLoad",             (void*)IMGLoad             },
		{ "DecodePNG8",          (void*)DecodePNG8          },
		{ "BlitSurface",         (void*)BlitSurface         },
		{ "CreatePaletteSurface",(void*)CreatePaletteSurface},
		{ "SetColorKey",         (void*)SetColorKey         },
		{ "AllocSurface",        (void*)AllocSurface        },
		{ "FreeSurface",         (void*)FreeSurface         },
		{ "Delay",               (void*)Delay               },
		{ "GetTicks",            (void*)GetTicks            },
		{ "CursorShow",          (void*)CursorShow          },
		{ "OpenFont",            (void*)OpenFont            },
		{ "CloseFont",           (void*)CloseFont           },
		{ "RenderFont",          (void*)RenderFont          },
		{ "SetSndBuf",           (void*)SetSndBuf           },
		{ "PlayBGM",             (void*)PlayBGM             },
		{ "PauseBGM",            (void*)PauseBGM            },
		{ "SendOpenBGM",         (void*)SendOpenBGM         },
		{ "SendCloseBGM",        (void*)SendCloseBGM        },
		{ "SendWriteBGM",        (void*)SendWriteBGM        },
		{ "SetVolume",           (void*)SetVolume           },
		{ "FadeInBGM",           (void*)FadeInBGM           },
		{ "FadeOutBGM",          (void*)FadeOutBGM          },
		{ "PlayVideo",           (void*)PlayVideo           },
		{ "RenderMugenZoom",     (void*)RenderMugenZoom     },
		{ "RenderMugenShadow",   (void*)RenderMugenShadow   },
		{ "Load8bitTexture",     (void*)Load8bitTexture     },
		{ "LoadPngTexture",      (void*)LoadPngTexture      },
		{ "DeleteGlTexture",     (void*)DeleteGlTexture     },
		{ "GlSwapBuffers",       (void*)GlSwapBuffers       },
		{ "InitMugenGl",         (void*)InitMugenGl         },
		{ "RenderMugenGl",       (void*)RenderMugenGl       },
		{ "RenderMugenGlFc",     (void*)RenderMugenGlFc     },
		{ "RenderMugenGlFcS",    (void*)RenderMugenGlFcS    },
		{ "MugenFillGl",         (void*)MugenFillGl         },
		{ "BindGlContext",       (void*)BindGlContext        },
		{ "UnbindGlContext",     (void*)UnbindGlContext      },
		{ "RenderFontBatch",     (void*)RenderFontBatch      },
	};

	return SSZ_RegisterFunction(
		"sdlplugin",
		sdlplugin_mapping,
		sizeof(sdlplugin_mapping) / sizeof(sdlplugin_mapping[0]));
}
