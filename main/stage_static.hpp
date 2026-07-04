// stage_static.hpp — Static plugin registration for ssz_script/ssz/stage.ssz
//
// When IKEMEN_NATIVE_STAGE_LIB=1, replaces the SSZ stage.ssz module
// with native C++ entry points registered in the static plugin table.

#pragma once
#include "static_plugin_registry.hpp"

#if IKEMEN_NATIVE_STAGE_LIB

#include <cstdint>
struct PluginUtil;   // forward (from pluginutil.hpp)
struct Reference;    // forward (from arrayandref.hpp)

extern "C" {

// init() — initializes stage data management.
void SSZ_STDCALL StageInit(PluginUtil* pu);

// load(def) — load a stage .def file. Returns error string (empty = success).
void SSZ_STDCALL StageLoad(PluginUtil* pu, Reference def, Reference* out);

// action() — advance stage animations one frame.
void SSZ_STDCALL StageAction(PluginUtil* pu);

// bgDraw(t, x, y, scl) — draw background layers for flag t.
void SSZ_STDCALL StageBgDraw(PluginUtil* pu, int32_t t, float x, float y, float scl);

// clear() — clear all stage data.
void SSZ_STDCALL StageClear(PluginUtil* pu);

// reset() — reset stage for next round.
void SSZ_STDCALL StageReset(PluginUtil* pu);

} // extern "C"

static inline bool stage_static_register()
{
	SSZ_FunctionEntry entries[] = {
		{ "init",    (void*)&StageInit    },
		{ "load",    (void*)&StageLoad    },
		{ "action",  (void*)&StageAction  },
		{ "bgDraw",  (void*)&StageBgDraw  },
		{ "clear",   (void*)&StageClear   },
		{ "reset",   (void*)&StageReset   },
	};
	return SSZ_RegisterFunction("stage", entries, 6);
}

#else

static inline bool stage_static_register()
{
	// IKEMEN_NATIVE_STAGE_LIB=0 — SSZ stage.ssz used instead.
	return true;
}

#endif
