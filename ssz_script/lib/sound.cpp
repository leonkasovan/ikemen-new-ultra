// ============================================================================
// sound.cpp — native (C++) implementation of the SSZ `sound` library's
// `&Client` struct methods.
//
// Consumed from ssz_script/lib/sound.ssz, which keeps the struct (field
// `index cl`) in SSZ and delegates the method bodies here:
//
//     lib sn = <sound>;
//     ...
//     public bool start() { ret .sn.clientStart(`cl); }
//
// The module is resolved by the native-lib registry (main/ssz/native_lib.hpp).
// The wrappers below forward to the clean native operations in
// main/sound/sound.cpp (the same functions the static bridge wraps).  The
// static `sound` plugin and this native lib coexist.
// ============================================================================

#include <cstdint>

#include "sszdef.h"
#include "native_lib.hpp"
#include "arrayandref.hpp"

struct PluginUtil;
struct Client;

// Clean natives defined in main/sound/sound.cpp
extern Client* SSZ_STDCALL NewClient();
extern void SSZ_STDCALL DeleteClient(Client* client);
extern bool SSZ_STDCALL ClientStart(Client* client);
extern bool SSZ_STDCALL ClientStop(Client* client);
extern bool SSZ_STDCALL ClientBufferReady(Client* client);
extern bool SSZ_STDCALL ClientSetBuffer(
	const float* buffer, intptr_t frames, Client* client);

// ---------------------------------------------------------------------------
// &Client struct methods — the struct stays in SSZ (field `index cl`), the
// method bodies delegate here with the handle passed by value or as an
// out-param for the zeroing case.  Args arrive reversed vs. the SSZ
// declaration.
// ---------------------------------------------------------------------------

// SSZ: &Client.new() -> index
static intptr_t SSZ_STDCALL SoundLibNew(PluginUtil*)
{
	return (intptr_t)NewClient();
}

// SSZ: &Client.delete() — delete the handle and zero the field.
static void SSZ_STDCALL SoundLibDelete(PluginUtil*, intptr_t* cl)
{
	if(*cl != 0){
		DeleteClient((Client*)*cl);
		*cl = 0;
	}
}

// SSZ: &Client.start() -> bool
static bool SSZ_STDCALL SoundLibStart(PluginUtil*, intptr_t cl)
{
	return ClientStart((Client*)cl);
}

// SSZ: &Client.stop() -> bool
static bool SSZ_STDCALL SoundLibStop(PluginUtil*, intptr_t cl)
{
	return ClientStop((Client*)cl);
}

// SSZ: &Client.bufferReady() -> bool
static bool SSZ_STDCALL SoundLibBufferReady(PluginUtil*, intptr_t cl)
{
	return ClientBufferReady((Client*)cl);
}

// SSZ: &Client.setBuffer(^/float buffer) -> bool
// Args arrive reversed: (pu, buffer, cl).
static bool SSZ_STDCALL SoundLibSetBuffer(
	PluginUtil*, Reference buffer, intptr_t cl)
{
	return ClientSetBuffer(
		(const float*)buffer.atpos(),
		buffer.len() / (intptr_t)sizeof(float),
		(Client*)cl);
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

extern "C" bool sound_lib_register()
{
	static const NativeLib::NativeFunction funcs[] = {
		{ "newClient",      "index ()",                  (void*)SoundLibNew         },
		{ "deleteClient",   "void (index=)",             (void*)SoundLibDelete      },
		{ "clientStart",    "bool (index)",              (void*)SoundLibStart       },
		{ "clientStop",     "bool (index)",              (void*)SoundLibStop        },
		{ "clientBufferReady", "bool (index)",           (void*)SoundLibBufferReady },
		{ "clientSetBuffer",   "bool (index, ^/float)",  (void*)SoundLibSetBuffer   },
	};

	NativeLib::NativeLibrary lib;
	lib.name = "sound";
	for(size_t i = 0; i < sizeof(funcs) / sizeof(funcs[0]); i++){
		lib.functions.push_back(funcs[i]);
	}
	return NativeLib::RegisterLibrary(lib);
}
