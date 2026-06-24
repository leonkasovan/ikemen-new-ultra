#pragma once
//
// static_plugin_registry.hpp
//
// Provides SSZ_RegisterFunction() — a mechanism for statically linking
// plugin functions instead of loading them from DLLs at runtime.
//
// Usage (C++ side, before SSZ compilation):
//
//   #include "static_plugin_registry.hpp"
//
//   // Forward-declare (or #include) the plugin functions:
//   extern "C" void SSZ_STDCALL Close(PluginUtil*, lua_State*);
//   extern "C" lua_State* SSZ_STDCALL NewState(PluginUtil*);
//   // ...
//
//   SSZ_FunctionEntry lua_mapping[] = {
//       { "Init",      (void*)Init      },
//       { "NewState",  (void*)NewState  },
//       { "Close",     (void*)Close     },
//       { "RunFile",   (void*)RunFile   },
//       // ...
//   };
//
//   if (!SSZ_RegisterFunction("lua", lua_mapping,
//           sizeof(lua_mapping)/sizeof(lua_mapping[0])))
//   {
//       // handle error
//   }
//
// The SSZ scripts continue to use their normal `plugin` declarations
// for type information, e.g.:
//   plugin void Close(:index:) = "dll/lua.dll";
//
// But the DLL file is no longer required — SDLLItem resolves the
// function pointer from the static registry instead of calling
// LoadLibrary / GetProcAddress.
//

#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <cctype>

// -----------------------------------------------------------------------
// Public data structures
// -----------------------------------------------------------------------

/// A single entry in the function mapping table.
struct SSZ_FunctionEntry
{
	const char* funcName;   // Exported function name, e.g. "Close"
	void*       funcPtr;    // Pointer to the function
};

// -----------------------------------------------------------------------
// Singleton registry
// -----------------------------------------------------------------------

class StaticPluginRegistry
{
public:
	static StaticPluginRegistry& instance();

	/// Register all functions for a library.
	/// @param libName  Logical library name, e.g. "lua".
	/// @param entries  Array of {funcName, funcPtr} pairs.
	/// @param count    Number of entries.
	/// @return true on success, false if any entry is invalid.
	bool registerFunctions(
		const char* libName,
		const SSZ_FunctionEntry* entries,
		size_t count)
	{
		if (!libName || !entries) return false;
		std::string lib = normalise(libName);
		auto& funcs = libraries_[lib];
		for (size_t i = 0; i < count; i++)
		{
			if (!entries[i].funcName || !entries[i].funcPtr) return false;
			funcs[entries[i].funcName] = entries[i].funcPtr;
		}
		LOG_DEBUG("SSZ", "Registered %zu functions for library '%s'", count, lib.c_str());
		return true;
	}

	/// Look up a single function by library name and exported name.
	void* lookupFunction(
		const std::string& libName,
		const std::string& funcName) const
	{
		auto lit = libraries_.find(normalise(libName));
		if (lit == libraries_.end()) {
			LOG_DEBUG("SSZ", "lookup: library '%s' not found", libName.c_str());
			return nullptr;
		}
		auto fit = lit->second.find(funcName);
		if (fit == lit->second.end()) {
			LOG_DEBUG("SSZ", "lookup: '%s' not found in library '%s'", funcName.c_str(), libName.c_str());
			return nullptr;
		}
		return fit->second;
	}

	/// Check whether a library has any statically registered functions.
	bool hasLibrary(const std::string& libName) const
	{
		return libraries_.find(normalise(libName)) != libraries_.end();
	}

	/// Extract the base name from a file path and normalise it.
	/// "C:\\game\\dll/lua.dll"  →  "lua"
	/// "dll/lua.dll"           →  "lua"
	/// "lua"                   →  "lua"
	static std::string extractBaseName(const std::string& path)
	{
		// Find the last path separator
		size_t sep = path.find_last_of("/\\");
		std::string name =
			(sep == std::string::npos) ? path : path.substr(sep + 1);
		// Strip extension
		size_t dot = name.find_last_of('.');
		if (dot != std::string::npos) name = name.substr(0, dot);
		// Lower-case for case-insensitive matching
		for (size_t i = 0; i < name.size(); i++)
			name[i] = (char)std::tolower((unsigned char)name[i]);
		return name;
	}

private:
	StaticPluginRegistry() {}
	StaticPluginRegistry(const StaticPluginRegistry&);
	StaticPluginRegistry& operator=(const StaticPluginRegistry&);

	static std::string normalise(const std::string& s)
	{
		std::string r = s;
		for (size_t i = 0; i < r.size(); i++)
			r[i] = (char)std::tolower((unsigned char)r[i]);
		return r;
	}
	static std::string normalise(const char* s)
	{
		return normalise(std::string(s));
	}

	// libName (lower-cased) → { funcName → funcPtr }
	std::unordered_map<
		std::string,
		std::unordered_map<std::string, void*>
	> libraries_;
};

// -----------------------------------------------------------------------
// Public C++ API
// -----------------------------------------------------------------------

/// Register an array of statically-linked plugin functions under the
/// given library name.  Call this before the SSZ compiler runs.
///
/// @param libName  Logical library name (e.g. "lua").  Case-insensitive.
/// @param entries  Array of {funcName, funcPtr} pairs.
/// @param count    Number of entries in the array.
/// @return true on success.
inline bool SSZ_RegisterFunction(
	const char* libName,
	const SSZ_FunctionEntry* entries,
	size_t count)
{
	return StaticPluginRegistry::instance()
		.registerFunctions(libName, entries, count);
}
