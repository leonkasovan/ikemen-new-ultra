// ============================================================================
// file.cpp — native (C++) implementation of the SSZ `file` library's module
// functions.
//
// Consumed from ssz_script/lib/file.ssz, which keeps the stateful `&File`
// struct (with its `|Seek` enum and the `readAll<_t>` template) in SSZ and
// delegates the plain module functions here:
//
//     lib fl = <file>;
//     ...
//     public ^char loadAsciiText(^/char fn) { ret .fl.loadAsciiText(fn); }
//
// The module is resolved by the native-lib registry (main/ssz/native_lib.hpp).
// Every function below follows the plugin ABI:
//
//     T name(PluginUtil*, ...)   (first arg is the SSZ runtime util; the JIT
//                                 passes g_gpsf.sf and reads the result from
//                                 the return slot)
//
// ABI note: the JIT pushes arguments in SSZ declaration order, so a C++
// function receives them reversed — the last SSZ parameter arrives first.
// `^/char` string params arrive as `Reference` values (null = empty); a list
// out-parameter (`%^char ls=`) arrives as a pointer to the caller's Reference
// slot, which the function fills in place — the same convention as the plugin
// bridges in main/ssz/bridge.cpp.
//
// The implementations forward to the clean native file operations in
// main/file/file.cpp (the same functions main/ssz/bridge.cpp wraps for the
// static `file` plugin).  The SSZ `&File` struct's `plugin ... = <file>`
// declarations still resolve to that static plugin, so both paths coexist.
// ============================================================================

#include <cstdint>
#include <string>
#include <vector>

#include "sszdef.h"
#include "native_lib.hpp"
#include "arrayandref.hpp"
#include "pluginutil.hpp"

struct PluginUtil;

// ---------------------------------------------------------------------------
// Native implementations defined in main/file/file.cpp
// ---------------------------------------------------------------------------

extern intptr_t SSZ_STDCALL Open(
	const std::wstring& md, const std::wstring& fn);
extern void SSZ_STDCALL FileClose(FILE* pFile);
extern bool SSZ_STDCALL Seek(int32_t origin, int64_t offset, FILE* pFile);
extern std::wstring SSZ_STDCALL LoadAsciiText(const std::wstring& path);
extern bool SSZ_STDCALL SaveAsciiText(
	const std::wstring& txt, const std::wstring& path);
extern bool SSZ_STDCALL Delete(const std::wstring& file);
extern bool SSZ_STDCALL Move(
	const std::wstring& newn, const std::wstring& oldn);
extern bool SSZ_STDCALL Copy(
	bool overwrite, const std::wstring& dist, const std::wstring& source);
extern std::vector<std::wstring> SSZ_STDCALL Find(const std::wstring& pattern);
extern std::vector<std::wstring> SSZ_STDCALL FindDir(const std::wstring& pattern);
extern bool SSZ_STDCALL CreateDir(const std::wstring& dir);
extern bool SSZ_STDCALL RemoveDir(const std::wstring& dir);
extern bool SSZ_STDCALL SetCurrentDir(const std::wstring& dir);
extern std::wstring SSZ_STDCALL GetCurrentDir();

// ---------------------------------------------------------------------------
// Helpers: Reference <-> native string conversions
// ---------------------------------------------------------------------------

// SSZ `^/char` param -> native std::wstring.
static std::wstring RefToWstring(Reference r)
{
#ifdef _WIN32
	return PluginUtil::refToWstr(r);   // WSTR == wstring
#else
	return PluginUtil::wToGw(PluginUtil::refToWstr(r));
#endif
}

// Build a heap Reference holding a `^char` string from a native wstring.
static intptr_t MakeStr(const std::wstring& w)
{
	Reference* r = (Reference*)sszrefnewfunc(sizeof(Reference));
	if(r != nullptr){
		r->init();
#ifdef _WIN32
		PluginUtil::wstrToRef(*r, w);
#else
		PluginUtil::wstrToRef(*r, PluginUtil::gwToW(w));
#endif
	}
	return (intptr_t)r;
}

// Fill a `%^char` list out-param with one Reference slot per item (mirrors
// ikemen::ssz_bridge::vectorToRefList in main/ssz/bridge.cpp).
static void MakeList(Reference* ls, const std::vector<std::wstring>& items)
{
	ls->releaseanddelete();
	if(items.empty()) return;
	for(size_t i = 0; i < items.size(); i++){
		intptr_t j = ls->addsize(1, sizeof(Reference), refzeroclearcb);
		Reference* slot = (Reference*)(ls->atpos() + j);
		slot->init();
#ifdef _WIN32
		PluginUtil::wstrToRef(*slot, items[i]);
#else
		PluginUtil::wstrToRef(*slot, PluginUtil::gwToW(items[i]));
#endif
	}
}

// ---------------------------------------------------------------------------
// Functions (plugin ABI — arguments arrive reversed vs. the SSZ declaration)
// ---------------------------------------------------------------------------

// SSZ: public ^char loadAsciiText(^/char fn)
static intptr_t SSZ_STDCALL FileLibLoadAsciiText(PluginUtil*, Reference fn)
{
	return MakeStr(LoadAsciiText(RefToWstring(fn)));
}

// SSZ: public bool saveAsciiText(^/char fn, ^/char txt)
static bool SSZ_STDCALL FileLibSaveAsciiText(
	PluginUtil*, Reference txt, Reference fn)
{
	return SaveAsciiText(RefToWstring(txt), RefToWstring(fn));
}

// SSZ: public bool remove(^/char fn)
static bool SSZ_STDCALL FileLibRemove(PluginUtil*, Reference fn)
{
	return Delete(RefToWstring(fn));
}

// SSZ: public bool move(^/char oldn, ^/char newn)
static bool SSZ_STDCALL FileLibMove(PluginUtil*, Reference newn, Reference oldn)
{
	return Move(RefToWstring(newn), RefToWstring(oldn));
}

// SSZ: public bool copy(^/char source, ^/char dist, bool overwrite)
static bool SSZ_STDCALL FileLibCopy(
	PluginUtil*, bool overwrite, Reference dist, Reference source)
{
	return Copy(overwrite, RefToWstring(dist), RefToWstring(source));
}

// SSZ: public ^^char find(^/char fn)  — list of file names
static void SSZ_STDCALL FileLibFind(
	PluginUtil* pu, Reference* ls, Reference fn)
{
	(void)pu;
	MakeList(ls, Find(RefToWstring(fn)));
}

// SSZ: public ^^char findDir(^/char fn)  — list of directory names
static void SSZ_STDCALL FileLibFindDir(
	PluginUtil* pu, Reference* ls, Reference fn)
{
	(void)pu;
	MakeList(ls, FindDir(RefToWstring(fn)));
}

// SSZ: public bool createDir(^/char dir)
static bool SSZ_STDCALL FileLibCreateDir(PluginUtil*, Reference dir)
{
	return CreateDir(RefToWstring(dir));
}

// SSZ: public bool removeDir(^/char dir)
static bool SSZ_STDCALL FileLibRemoveDir(PluginUtil*, Reference dir)
{
	return RemoveDir(RefToWstring(dir));
}

// SSZ: public bool setCurrentDir(^/char cdir)
static bool SSZ_STDCALL FileLibSetCurrentDir(PluginUtil*, Reference cdir)
{
	return SetCurrentDir(RefToWstring(cdir));
}

// SSZ: public ^/char getCurrentDir()
static intptr_t SSZ_STDCALL FileLibGetCurrentDir(PluginUtil*)
{
	return MakeStr(GetCurrentDir());
}

// ---------------------------------------------------------------------------
// &File struct methods — the struct stays in SSZ (field `index fh`), the
// method bodies delegate here with the field passed as an `index=` out-param
// (pointer to the caller's 8-byte slot, read-write).
// ---------------------------------------------------------------------------

// SSZ: &File.open(^/char fn, ^/char mode) — closes any open handle first.
// Args arrive reversed: (pu, mode, fn, fh*).
static bool SSZ_STDCALL FileLibFileOpen(
	PluginUtil*, Reference mode, Reference fn, intptr_t* fh)
{
	if(*fh != 0){
		FileClose((FILE*)*fh);
		*fh = 0;
	}
	*fh = Open(RefToWstring(mode), RefToWstring(fn));
	return *fh != 0;
}

// SSZ: &File.close()
static void SSZ_STDCALL FileLibFileClose(PluginUtil*, intptr_t* fh)
{
	if(*fh != 0){
		FileClose((FILE*)*fh);
		*fh = 0;
	}
}

// SSZ: &File.isOpened()
static bool SSZ_STDCALL FileLibFileIsOpened(PluginUtil*, intptr_t fh)
{
	return fh != 0;
}

// SSZ: &File.seek(long offset, |.Seek origin)
// Args arrive reversed: (pu, origin, offset, fh).
static bool SSZ_STDCALL FileLibFileSeek(
	PluginUtil*, int32_t origin, int64_t offset, intptr_t fh)
{
	return Seek(origin, offset, (FILE*)fh);
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

extern "C" bool file_lib_register()
{
	static const NativeLib::NativeFunction funcs[] = {
		{ "loadAsciiText", "^char (^/char)",               (void*)FileLibLoadAsciiText },
		{ "saveAsciiText", "bool (^/char, ^/char)",        (void*)FileLibSaveAsciiText },
		{ "remove",        "bool (^/char)",                (void*)FileLibRemove        },
		{ "move",          "bool (^/char, ^/char)",        (void*)FileLibMove          },
		{ "copy",          "bool (^/char, ^/char, bool)",  (void*)FileLibCopy          },
		{ "find",          "void (^/char, %^char=)",       (void*)FileLibFind          },
		{ "findDir",       "void (^/char, %^char=)",       (void*)FileLibFindDir       },
		{ "createDir",     "bool (^/char)",                (void*)FileLibCreateDir     },
		{ "removeDir",     "bool (^/char)",                (void*)FileLibRemoveDir     },
		{ "setCurrentDir", "bool (^/char)",                (void*)FileLibSetCurrentDir },
		{ "getCurrentDir", "^/char ()",                    (void*)FileLibGetCurrentDir },
		{ "fileOpen",      "bool (index=, ^/char, ^/char)", (void*)FileLibFileOpen      },
		{ "fileClose",     "void (index=)",                 (void*)FileLibFileClose     },
		{ "fileIsOpened",  "bool (index)",                  (void*)FileLibFileIsOpened  },
		{ "fileSeek",      "bool (index, long, int)",       (void*)FileLibFileSeek      },
	};

	NativeLib::NativeLibrary lib;
	lib.name = "file";
	for(size_t i = 0; i < sizeof(funcs) / sizeof(funcs[0]); i++){
		lib.functions.push_back(funcs[i]);
	}
	return NativeLib::RegisterLibrary(lib);
}
