// sdlplugin_service.hpp — Native C++ scaffolding for ssz_script/lib/alpha/sdlplugin.ssz
//
// sdlplugin.ssz (1022 lines) implements SDL rendering wrappers — Flip, Fill,
// SoftFill, RenderMugenZoom, BlitSurface, font rendering, sprite loading,
// and all SDL plugin entry points.
//
// All functions are thin wrappers around bridge calls to the native SDL
// plugin (main/sdlplugin/sdlplugin.cpp). The native implementations already
// exist; this file provides the SSZ script-layer scaffolding.

#pragma once

namespace ikemen::ssz_native {

struct SdlPluginState {
	// Phase 2 deferred: populated when SDL plugin conversions complete.
};

} // namespace ikemen::ssz_native
