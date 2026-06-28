#pragma once
//
// math_static.hpp
//
// Statically register every function exported by math.cpp
// so that the SSZ runtime resolves them without loading math.dll.
//
// When IKEMEN_NATIVE_MATH_LIB=1, the native math_service is active
// and bridge registration can be skipped. See Makefile for flag docs.

#include "static_plugin_registry.hpp"

#if IKEMEN_NATIVE_MATH_LIB

struct PluginUtil;

extern "C"
{
	double SSZ_STDCALL Sin(PluginUtil*, double);
	double SSZ_STDCALL Cos(PluginUtil*, double);
	double SSZ_STDCALL Tan(PluginUtil*, double);
	double SSZ_STDCALL ASin(PluginUtil*, double);
	double SSZ_STDCALL ACos(PluginUtil*, double);
	double SSZ_STDCALL ATan(PluginUtil*, double);
	double SSZ_STDCALL Log(PluginUtil*, double, double);
	double SSZ_STDCALL Ln(PluginUtil*, double);
	double SSZ_STDCALL Exp(PluginUtil*, double);
	double SSZ_STDCALL Sqrt(PluginUtil*, double);
	double SSZ_STDCALL Ceil(PluginUtil*, double);
	double SSZ_STDCALL Floor(PluginUtil*, double);
	bool   SSZ_STDCALL IsFinite(PluginUtil*, double);
	bool   SSZ_STDCALL IsInf(PluginUtil*, double);
	bool   SSZ_STDCALL IsNaN(PluginUtil*, double);
}

inline bool math_static_register()
{
	static const SSZ_FunctionEntry math_mapping[] =
	{
		{ "Sin",      (void*)Sin      },
		{ "Cos",      (void*)Cos      },
		{ "Tan",      (void*)Tan      },
		{ "ASin",     (void*)ASin     },
		{ "ACos",     (void*)ACos     },
		{ "ATan",     (void*)ATan     },
		{ "Log",      (void*)Log      },
		{ "Ln",       (void*)Ln       },
		{ "Exp",      (void*)Exp      },
		{ "Sqrt",     (void*)Sqrt     },
		{ "Ceil",     (void*)Ceil     },
		{ "Floor",    (void*)Floor    },
		{ "IsFinite", (void*)IsFinite },
		{ "IsInf",    (void*)IsInf    },
		{ "IsNaN",    (void*)IsNaN    },
	};

	return SSZ_RegisterFunction(
		"math",
		math_mapping,
		sizeof(math_mapping) / sizeof(math_mapping[0]));
}

#else
inline bool math_static_register() { return true; }
#endif
