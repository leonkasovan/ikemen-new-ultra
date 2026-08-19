# SSZ → Native Library Conversion — Complete Reference

This document captures all knowledge accumulated during the conversion of the
SSZ script libraries to native C++ implementations. It is intended as a
standalone reference for anyone working on the SSZ→native boundary, the native
library registry, or the JIT compiler's interaction with native code.

---

## Table of Contents

1. [Overview](#overview)
2. [Architecture](#architecture)
3. [Plugin ABI — The Contract](#plugin-abi--the-contract)
4. [Writing a Native Library](#writing-a-native-library)
5. [Signature String Reference](#signature-string-reference)
6. [The `&X` Struct-Method Delegation Pattern](#the-struct-method-delegation-pattern)
7. [Alpha Bridge Libs (`lib/alpha/*`)](#alpha-bridge-libs)
8. [Template Libraries — Why They Stay in SSZ](#template-libraries--why-they-stay-in-ssz)
9. [JIT Extensions for Native Support](#jit-extensions-for-native-support)
10. [Known Gotchas and Fixes](#known-gotchas-and-fixes)
11. [Testing](#testing)
12. [Conversion Status](#conversion-status)
13. [Game Script Conversion (SSZ → Native C++)](#game-script-conversion-ssz--native-c)
14. [Commit History](#commit-history)

---

## Overview

Ikemen GO uses a custom JIT-compiled scripting language called **SSZ**. The
engine has a static plugin architecture where 14 subsystems register exported
functions with the SSZ runtime. Libraries (`lib ... = <name>;` statements)
can be implemented as:

| Form | Resolution |
|---|---|
| `lib time = <time.ssz>;` | Read and compile the script file `lib/time.ssz` |
| `lib time = <time>;` (no `.ssz`) | Native C++ via `NativeLib::FindLibrary("time")` — falls back to file read if not registered |

The conversion effort progressively replaced `.ssz` library modules with native
C++ functions backed by the native library registry (`main/ssz/native_lib.hpp`),
while keeping template-bound code in SSZ.

**Final result:** 20 native libraries registered (16 conversions + 4 alpha
bridges + 2 test fixtures), 17 regression tests, full game boots in Debug
and Release. Plus 2 game scripts (video.ssz, sound.ssz) converted to native
C++ via the SSZ-to-C++ transpiler pattern.

---

## Architecture

### Three-Layer Resolution

```
SSZ Script                    JIT Compiler                 Native C++
─────────                     ────────────                 ───────────
lib time = <time>;     →     NativeLib::FindLibrary("time")
                                  ↓
                          NativeLibrary.functions[]    →    TickCount(PluginUtil*, ...)
                                  ↓
                          hensuu synthesized from signature string
                                  ↓
                          type-checked + JIT-compiled
```

### Key Files

| File | Role |
|---|---|
| `main/ssz/native_lib.hpp` | Native library registry — `RegisterLibrary`, `FindLibrary`, `TypeNameToTokens` |
| `main/ssz/jitcompiler.hpp` | JIT compiler — `KansuuPointer` (call-site binding), type inference, code generation |
| `main/ssz/sourcetree.hpp` | Source tree — `SDLLItem` resolution, `lib` statement parsing, `NativeLib` import |
| `main/ssz/bridge.cpp` | Plugin ABI bridge wrappers (existing static plugins) |
| `main/ssz/tokenkind.h` | Token definitions — `TYPE_TOKEN`, `REF_TOKEN`, `FUNC_TOKEN`, `DYNREF` |
| `main/main.cpp` | Registration calls (`*_lib_register()`) and plugin registration |
| `Makefile` | `NATIVE_LIB_SRCS` lists all native library sources |

### Registration Flow

1. Each native lib exports `extern "C" bool <name>_lib_register()`
2. `main/main.cpp` calls it before the SSZ compiler runs
3. Functions are registered as `{ name, "ssz-signature", fnptr }` triples
4. When `lib time = <time>;` is compiled, the compiler calls
   `NativeLib::FindLibrary("time")` and synthesizes henshuu from the
   signature strings

---

## Plugin ABI — The Contract

All native functions follow the same ABI, proven by the plugin bridges in
`main/ssz/bridge.cpp` and validated across 20 native libraries.

### Function Signature

```c
static <return_type> SSZ_STDCALL FunctionName(PluginUtil* pu, <params...>);
```

The first parameter is **always** `PluginUtil*` — it is invisible to SSZ
scripts and not counted in the signature string.

### Argument Reversal

**Arguments arrive reversed** — the last SSZ parameter is the first C++
parameter:

```ssz
// SSZ declaration
plugin bool Open(^/char file, ^/char arg, ^/char cdir, bool waitfor, bool active);
```
```c
// C++ receives them reversed
static int32_t SSZ_STDCALL ShellLibOpen(
    PluginUtil* pu,
    int32_t active,     // last SSZ param → first C++ param after pu
    int32_t waitfor,
    Reference cdir,
    Reference arg,
    Reference file      // first SSZ param → last C++ param
);
```

### 32-Bit Slots

32-bit SSZ args (`int`/`uint`/`bool`/`float`/...) occupy the **low 32 bits
of an 8-byte slot** with unspecified high bits. Declare them as `int32_t` /
`uint32_t` / `float`, **never `int64_t`** (which would read garbage from the
high bits).

| SSZ Type | C++ Type | Size |
|---|---|---|
| `int`, `index` | `int32_t` | 4 bytes (low 32 of 8-byte slot) |
| `uint` | `uint32_t` | 4 bytes |
| `bool` | `int32_t` | 4 bytes (0 or 1) |
| `float` | `float` | 4 bytes |
| `long` | `int64_t` | 8 bytes (full slot) |
| `ulong` | `uint64_t` | 8 bytes |
| `pointer` | `intptr_t` | 8 bytes |

### String Parameters

Strings arrive as `Reference` — convert with:
```cpp
std::wstring wstr = ikemen::ssz_bridge::refToWstring(pu, ref);
```

### Out-Parameters (`type=`)

The C function receives a **pointer to the caller's slot**:

```ssz
// SSZ: index fh=
// C++: receives int32_t* fh — write the result through the pointer
static void SSZ_STDCALL FileOpen(PluginUtil* pu, int32_t* fh, Reference fn, Reference mode) {
    *fh = OpenFile(fn_wstr, mode_wstr);
}
```

The JIT copies the written value back to the caller's variable. A native
function whose SSZ declaration has `=` but takes the value by copy silently
fails to write back.

### String/Array Returns (`^char`, `^/char`, `^ubyte`)

Return the address of a heap-allocated `Reference`:

```cpp
// String return
Reference* ref = (Reference*)sszrefnewfunc(sizeof(Reference));
ref->init();
PluginUtil::wstrToRef(pu, ref, result_wstr);
return (intptr_t)ref;

// Byte array return
Reference* ref = (Reference*)sszrefnewfunc(sizeof(Reference));
ref->init();
refnew(ref, size, 1);
memcpy(ref->ptr, data, size);
return (intptr_t)ref;

// Null/empty return
return 0;
```

The JIT unpacks the returned struct's fields (pointer/position/length) into the
temp-ref registers.

### Module Variables

Native libraries may expose module variables (`NativeVariable`) — registered as
ordinary SSZ module variables (e.g. `"public int"` for cross-module-visible),
backed by the module's variable frame. The C++ function holds internal state
as `static`; the registered variable is for SSZ-side interface parity.

---

## Writing a Native Library

### Step-by-Step

1. **Create `ssz_script/lib/<name>.cpp`** with `extern "C" bool <name>_lib_register()`
2. **Add to `NATIVE_LIB_SRCS`** in the Makefile
3. **Declare and call `<name>_lib_register()`** in `main/main.cpp`
4. **Change consuming scripts** to `lib <name> = <name>;` (drop `.ssz` extension)
5. **Write regression test** in `test/ssz/<name>test.ssz`

### Template

```cpp
// ssz_script/lib/mylib.cpp
//
// Native core for the SSZ `mylib` library.
// Consumed as: lib ml = <mylib>;

#include <cstdint>
#include <string>
#include "sszdef.h"
#include "bridge.hpp"
#include "native_lib.hpp"

struct PluginUtil;
struct Reference;

// SSZ: public uint myFunc(uint x, ^/char str)
//
//   Args arrive reversed: str is first C++ param after pu, x is second.
static uint32_t SSZ_STDCALL MyFunc(PluginUtil* pu, Reference str, uint32_t x) {
    std::wstring wstr = ikemen::ssz_bridge::refToWstring(pu, str);
    // ... implementation ...
    return result;
}

// SSZ: public ^char myStringFunc(^/char input)
//
//   Returns a heap-allocated Reference.
static intptr_t SSZ_STDCALL MyStringFunc(PluginUtil* pu, Reference input) {
    std::wstring wstr = ikemen::ssz_bridge::refToWstring(pu, input);
    // ... transform ...
    Reference* ref = (Reference*)sszrefnewfunc(sizeof(Reference));
    ref->init();
    PluginUtil::wstrToRef(pu, ref, result);
    return (intptr_t)ref;
}

// SSZ: public void myOutFunc(index x=, ^char s=)
//
//   Out-params arrive as pointers.
static void SSZ_STDCALL MyOutFunc(PluginUtil* pu, int32_t* x, Reference* s) {
    *x = 42;
    PluginUtil::wstrToRef(pu, s, L"hello");
}

extern "C" bool mylib_lib_register() {
    NativeLib::NativeFunction funcs[] = {
        { "myFunc",        "uint (uint, ^/char)",     (void*)MyFunc },
        { "myStringFunc",  "^char (^/char)",           (void*)MyStringFunc },
        { "myOutFunc",     "void (index=, ^char=)",    (void*)MyOutFunc },
    };
    NativeLib::NativeLibrary lib;
    lib.name = "mylib";
    for (auto& f : funcs) lib.functions.push_back(f);
    return NativeLib::RegisterLibrary(lib);
}
```

### Calling Static Plugin Functions from Native Libs

If your native lib wraps an existing static plugin (like the alpha bridges),
you can call the plugin functions directly since they're linked in:

```cpp
#include "sdlplugin_plugin.hpp"

// The static plugin functions are available as regular C++ symbols
extern "C" {
    int32_t SSZ_STDCALL Delay(PluginUtil*, uint32_t ms);
    int32_t SSZ_STDCALL GetTicks(PluginUtil*);
}
```

---

## Signature String Reference

Signature strings are tokenized by `TypeNameToTokens` in `main/ssz/native_lib.hpp`
into plugin-typed henshuu. The JIT uses these to type-check and compile calls
exactly like `plugin` calls.

### Basic Types

| SSZ Type | Signature | Token |
|---|---|---|
| `int`, `index` | `"int"` | `INT_TOKEN` |
| `uint` | `"uint"` | `UINT_TOKEN` |
| `bool` | `"bool"` | `BOOL_TOKEN` |
| `float` | `"float"` | `FLOAT_TOKEN` |
| `long` | `"long"` | `LONG_TOKEN` |
| `ulong` | `"ulong"` | `ULONG_TOKEN` |
| `pointer` | `"pointer"` | `POINTER_TOKEN` |

### Reference Types

| SSZ Form | Signature | Token Sequence |
|---|---|---|
| `^char` | `"^char"` | `REF_TOKEN` + `CHAR_TOKEN` |
| `^/char` (const ref) | `"^/char"` | `REF_TOKEN` + `WCHR_TOKEN` |
| `^ubyte` | `"^ubyte"` | `REF_TOKEN` + `UBYTE_TOKEN` |
| `^^char` (ref to ref) | `"^^char"` | `REF_TOKEN` + `REF_TOKEN` + `CHAR_TOKEN` |

### Pointer Types

| SSZ Form | Signature | Token |
|---|---|---|
| `^int` | `"^int"` | `REF_TOKEN` + `INT_TOKEN` |
| `^uint` | `"^uint"` | `REF_TOKEN` + `UINT_TOKEN` |

### Out-Parameters

Append `=` to the type in the signature:

| SSZ Form | Signature | Notes |
|---|---|---|
| `index i=` | `"index="` | `int32_t*` in C++ |
| `^char s=` | `"^char="` | `Reference*` in C++ |
| `^ubyte buf=` | `"^ubyte="` | `Reference*` in C++ |
| `float x=` | `"float="` | `float*` in C++ |
| `ref r=` | `"ref="` | `DynamicRef*` in C++ |

### Enum and Struct Types

Introduced by the `|Enum`/`&Struct` support in `TypeNameToTokens`:

| SSZ Form | Signature | Encoding |
|---|---|---|
| `\|SDLKey` | `"|SDLKey"` | `AND_TOKEN` + class id |
| `\|CodePage` | `"|CodePage"` | `AND_TOKEN` + class id |
| `&Event` | `"&Event"` | `OR_TOKEN` + class id |

The class id is resolved via `NativeTypeContext::resolveClassId(name)` — the
importing module must have declared the type before the `lib` import.

### Generic Type (`_t`)

| SSZ Form | Signature | Token |
|---|---|---|
| `_t` | `"_t"` | `TYPE_TOKEN` |

The JIT binds `_t` from the first call-site argument whose parameter mentions
`TYPE_TOKEN`, then substitutes every `TYPE_TOKEN` in the signature (params and
return) before type-checking and code emission. This happens per call site
(`KansuuPointer` in `jitcompiler.hpp`).

### Delegate Types

| SSZ Form | Signature | Encoding |
|---|---|---|
| `ref` | `"ref"` | `REF_TOKEN` + `NULL_TOKEN` (= `DYNREF_TYPEID`) |
| `func$void(int)` | `"func$void(int)"` | `FUNC_TOKEN` + `SIGNATURE_TOKEN` + `VOID_TOKEN` + `(` + `INT_TOKEN` + `)` |
| `func$void(int=)` | `"func$void(int=)"` | `FUNC_TOKEN` + `SIGNATURE_TOKEN` + `VOID_TOKEN` + `(` + `INT_TOKEN` + `=` + `)` |
| `^null` (DYNREF) | `"^null"` | `REF_TOKEN` + `NULL_TOKEN` |

C++ types for delegates:
- `ref=` out-params → `DynamicRef*`
- `ref` values → `DynamicRef`
- `func` values → `intptr_t` (delegate slot)

### Array/Container Types

| SSZ Form | Signature | Notes |
|---|---|---|
| `%int` (dynamic array) | `"%int"` | `WAKAME_TOKEN` + type |
| `^/ubyte` (const byte ref) | `"^/ubyte"` | Used for byte array params |

### Mixed Signatures (Real Examples)

```
"uint ()"                              — TickCount()
"long ()"                              — UnixTime()
"void ()"                              — Delay(uint ms) — but ms is uint
"void (uint)"                          — Sleep(uint ms)
"bool (^/char, ^/char)"               — Equ(^/char a, ^/char b)
"^char (^/char)"                       — Trim(^/char s) → returns string
"^char (ulong)"                        — UToSo(ulong v) → hex string
"void (long, int=)"                    — Seek(long offset, int origin=)
"^char= (^/char, ^char=)"             — GetInifileString(file, key, app, default, result=)
"bool (^/char, ^/char, ^/char, ^/char, ^char=)" — IniString(file, app, key, default, result=)
"void (|CodePage, ^char=, ^/ubyte=)"  — UbytesToStr(codepage, str=, bytes=)
"^ubyte (^/ubyte)"                    — Md5(^/ubyte data) → digest
"void (^/ubyte=, ^/ubyte)"            — Md5Start(^/ubyte digest=, ^/ubyte data)
"long (^/ubyte=, long=, index, ^/ubyte)" — ReadAry(buf=, offset=, size, data)
"ref (^/char)"                         — CompileString(^/char src) → ref
"void (index, ^/char)"                — Run(ref compiler, ^/char name)
```

---

## The `&X` Struct-Method Delegation Pattern

Stateful structs (`&File`, `&Md5`, `&Arcfour`, `&Regex`, `&Client`,
`&Compiler`, `&Socket`) stay defined in their `.ssz` as data containers —
field declarations, `new()` allocation, templates, and whole-object
manipulation stay in SSZ — while their non-template method bodies delegate
to the native lib.

### How It Works

```ssz
lib fl = <file>;

public &File
{
  index fh = 0;
  public bool open(^/char fn, ^/char mode)
  {
    ret .fl.fileOpen(`fh=, fn, mode);   // field passed as out-param
  }
  public void close()
  {
    .fl.fileClose(`fh=);
  }
}
```

Key points:
- **Fields are passed as out-params** — `` `fh= `` passes a pointer to the
  `fh` field; the C++ side writes the result through it
- **The `lib` import must appear above the struct** so method bodies can
  reference it
- **Template methods** keep their in-body `plugin` declarations — the static
  plugin registry and native registry coexist
- **`^` prefix** on field references (`^fh`) creates a reference-to-field;
  **backtick** (`` ` ``) passes the field as an out-param

### Out-Param Conventions by Type

| SSZ Field | C++ Parameter | Write Target |
|---|---|---|
| `` `fh `` (index) | `int32_t* fh` | `*fh = value` |
| `` `buf `` (^ubyte) | `Reference* buf` | `refnew(buf, size, 1); memcpy(buf->ptr, ...)` |
| `` `count `` (^uint) | `Reference* count` | Write size as uint32 into the ref |
| `` `s `` (^char) | `Reference* s` | `PluginUtil::wstrToRef(pu, s, str)` |

---

## Alpha Bridge Libs

`ssz_script/lib/alpha/` holds bridge/type-definition libs, not logic libs.
Each is a thin bridge to an already-static plugin plus the enums/structs
that form the game's type vocabulary.

### sdlplugin (✅ converted)

Re-exports the static `<sdlplugin>` plugin's fnptrs. The `.ssz` keeps
enums (`|EventType`, `|SDLKey`, `|K`), structs (`&Event`, `&Rect`), and
constants. Module functions delegate natively. `&Surface`/`&Font`/`&GlTexture`
methods keep in-body plugin declarations.

**Key rule:** The importing module must declare referenced types **before**
the `lib sdlp = <sdlplugin>;` import.

### sdlevent (✅ converted — timing core)

`&Key` methods (`reset`/`checkDown`, dot-qualified `|.sdl.K` enum params)
AND the `event(fps)` timing core delegate to native code. The timing core
(`eventTiming(fps, now, nexttime=, lastdraw=, frac=, fskip=)`) is a faithful
transcription of the original `branch` — all conds are uint wraparound
arithmetic.

`now` is passed in by the wrapper (`.sdl.GetTicks(::)`) so the arithmetic is
deterministic and regression-testable.

The stateful `eventUpdate()` pump, `sdle` event struct, and key-flag clears
stay in SSZ (module-variable state is the public `.se.*` API).

### lua (✅ converted)

Re-exports the static `<lua>` plugin's fnptrs. Enabled by `ref`/`func`
support in `TypeNameToTokens`. `&State` methods delegate natively (`ref=`
out-params arrive as `DynamicRef*`, `func` values as `intptr_t`).

Two methods stay in SSZ:
- `init()` — passes `func X.signature` *signature* values, not delegates
- `register` — its `func$void(self=, int=)` param captures the enclosing
  class (per-instantiation class id a native signature cannot name)

### mesdialog (✅ converted)

Re-exports the static `<mesdialog>` plugin's fnptrs. Enabled by `|CodePage`
enum support in `TypeNameToTokens`. Module functions delegate natively.
`veryUnsafeCopy` stays in SSZ (template-bound).

**Key rule:** The `|CodePage` enum must be declared **before** the
`lib md = <mesdialog>;` import — its type is referenced by the native sigs.

### ogg (🗑 removed)

Was dead code — `ssz/sound.ssz` dropped the import ("SDL_mixer handles OGG"),
and converting it would only duplicate the static `<ogg>` plugin it bridged.

---

## Template Libraries — Why They Stay in SSZ

### The Fundamental Constraint

SSZ templates work by **re-parsing the template body from source** with the
template parameter bound to a concrete type (`BlockOpen`/`MakeTree` in
`sourcetree.hpp`). This re-parse happens per instantiation — e.g.,
`probe!int?` re-parses the body with `_t` bound as `TYPE_TOKEN + int`.

Native library signatures are **synthesized once at import** — they cannot be
re-parsed per instantiation. This means:

1. **Template-typed fields** (`^_t data`, `&Table!_t, node_t? t`) cannot be
   expressed in a native signature
2. **Template return types** with no parameters to infer from (`pop()` returns
   `^_t` with no arguments) cannot bind `_t`
3. **Per-instantiation class types** (`&Node!int`, `&Stack!float`) cannot be
   named in a native signature

### Why `stack` Was Removed

`&Stack<_t>`/`&Node<_t>` — all four reasons still apply after `_t` and
`ref`/`func` landed:

1. Fields are template-typed (`^_t data`, `^&Node!_t? topNode`)
2. `pop()`/`top()` return `^_t` with no parameters to bind from
3. Method bodies are pure SSZ heap/ref manipulation — no concrete C++ core
4. **Zero consumers** in the codebase — removed as dead code

### Why `table` Can't Go Further

`&NameTable<_t>`/`&IntTable<int_t,_t>` — same template-typed fields
(`^_t data`, `&Table!_t, node_t? t`). The `func` delegate params
(`each`/`forEach`/`operate`) are now expressible, but the fields are the
hard blocker. Only `hash` (concrete, no templates) is native.

### Why `consts` / `alert` Stay

- `consts.ssz` — pure type-system sugar (`&Signed<_t>`, `&Unsigned<_t>`,
  `null<_t>`)
- `alert.ssz` — 3-line wrapper; `_t` only feeds compile-time `typeid(_t)`

### What CAN Use Native `_t`

The `tmpl` test fixture proves native `_t` signatures work for **function
calls** where `_t` can be inferred from a parameter:

```cpp
// SSZ: _t min(_t a, _t b)
// JIT binds _t from the first argument, substitutes through signature
static int64_t SSZ_STDCALL TmplMin(PluginUtil*, int64_t a, int64_t b) {
    return a < b ? a : b;
}
```

Supported patterns (verified by `tmpltest`):
- Scalar params/returns: `_t min(_t, _t)`
- Out-params: `void inc(_t v=)`
- Array out-params: `void fill(^_t buf=, _t val)`
- Ref returns: `^_t make(_t val)`
- `long` (8-byte) instantiations

---

## JIT Extensions for Native Support

### 1. `|Enum`/`&Struct` in `TypeNameToTokens` (commit `6f49edd`)

`TypeNameToTokens` now handles `|Enum` and `&Struct` types in signature
strings. They're encoded as `AND_TOKEN`/`OR_TOKEN` + the type's funclist
class id, resolved via a `NativeTypeContext` resolver callback the importing
module supplies.

```cpp
struct NativeTypeContext {
    // Resolve a type name to its class id (like PathtoClassID)
    // Returns -1 if not found
    int (*resolveClassId)(const wchar_t* name, void* userdata);
    void* userdata;
};
```

**Usage in signature strings:**
```
"|SDLKey"     → AND_TOKEN + class_id(SDLKey)
"|CodePage"   → AND_TOKEN + class_id(CodePage)
"&Event"      → OR_TOKEN  + class_id(Event)
"&Rect"       → OR_TOKEN  + class_id(Rect)
```

**C++ parameter types for enums:** `int32_t` (the enum is stored as an int).

### 2. Native `_t` (TYPE_TOKEN) Call-Site Inference (commit `f6526b2`)

When a native signature contains `TYPE_TOKEN`, the JIT's `KansuuPointer`
function:

1. Scans parameters for the first one mentioning `TYPE_TOKEN`
2. Binds `_t` to that argument's compiled type
3. Substitutes every `TYPE_TOKEN` in the signature (params and return)
4. Type-checks and emits code with the concrete types

This happens **per call site** — different calls to the same native function
can use different concrete types. The native C++ implementation must handle
one specific element type per function (e.g., `int64_t` for `long`, `int32_t`
for `int`).

### 3. `ref`/`func` Delegate Support (commit `3075c15`)

`TypeNameToTokens` encodes:
- `ref` → `REF_TOKEN` + `NULL_TOKEN` (= `DYNREF_TYPEID`)
- `^null` → `REF_TOKEN` + `NULL_TOKEN`
- `func$ret(params)` → `FUNC_TOKEN` + `SIGNATURE_TOKEN` + ret + `(` + params + `)`

Parameter splitting in `BuildPluginType` is paren-depth-aware (func params
contain commas). A top-level-`(` scan tolerates func-typed returns.

**C++ types:**
- `ref=` out-params → `DynamicRef*` (pointer to 4-byte heap cell)
- `ref` values → `DynamicRef` (4-byte heap cell value)
- `func` values → `intptr_t` (delegate slot / function pointer)

### 4. `|CodePage` Enum Signatures (commit `81b3fca`)

Same mechanism as `|SDLKey` — the `NativeTypeContext` resolver resolves
`CodePage` to its class id. The enum must be declared in the importing
script before the `lib` import.

---

## Known Gotchas and Fixes

### `(_t)` vs `(*_t)` Casts in Templates

Bare `(_t)` casts fail at template instantiation unless the operand is
already `_t`-typed. `(*_t)` casts work universally.

```ssz
// BROKEN — fails at instantiation
(_t)ub[i]

// WORKS — universal cast
(*_t)ub[i]
```

**Fixed in:** `decBase64<_t>` byte path (`ssz_script/lib/base64.ssz`).

### Lua FFI `EnableExecute` Read-After-Flip

The Lua FFI's `EnableExecute` function uses `VirtualProtect(data, size,
PAGE_EXECUTE, &old)` — but `PAGE_EXECUTE` (0x10) is **execute-only** on x86
Windows. When the caller passes `page->size` (a read through the page being
flipped), a compiler that re-loads the value after the call faults.

**Root cause:** `commit_code` in `call.c:218` — deterministic in Debug, also
reproducible in Release at a later call site. Manifested as `luaL_newstate`
crash inside `luaopen_ffi → compile_globals → commit_code`.

**Fix:** Capture `size` in a local **before** the flip:
```c
size_t _sz = (size);
VirtualProtect((data), _sz, PAGE_EXECUTE, &old);
FlushInstructionCache(GetCurrentProcess(), (data), _sz);
```

**File:** `external/lua-5.2.4/ffi/ffi.h` (Windows branch).

### `md5str(digest)` Re-Hashes

`md5str` hashes its input — passing a digest through `md5str` re-hashes it.
Use `toHex!ubyte?(digest)` to render a digest as hex.

### `uint` Literals in Templates

`uint` literals like `tmp = 1;` fail inside templates — strict int→uint
literal rules apply. Use `tmp = 0x1;` instead.

### Enum Type Must Be Declared Before `lib` Import

When a native signature references an enum type (e.g., `|CodePage`), the
importing script must declare that type **before** the `lib` import:

```ssz
|CodePage { ACP = 0; OEMCP = 1; UTF8 = 65001; }  // MUST come first
lib md = <mesdialog>;  // native sigs reference |CodePage
```

The same rule applies to `|SDLKey` in sdlplugin.

### Module-Level Out-Params Use Dot Prefix

When passing module-level variables as out-params to native functions:

```ssz
// Module vars — use .nexttime=
.evt.eventTiming(fps, .sdl.GetTicks(::), .nexttime=, .lastdraw=,
                 .nexttimeFractionalPart=, .fskip=);

// Function params — plain =
public void foo(index x=, ^char s=) { ... }

// Struct fields — backtick
`fh=
```

### Test Scripts Must Live in `test/ssz/`

The exe resolves `lib/x.ssz` relative to its own directory. Pass an absolute
path to the script when running from `test/work/`:

```bash
cd test/work && ../../install/ikemen-debug.exe "$(cygpath -w ../ssz/filetest.ssz)"
```

### `print` Doesn't Exist at Root Scope

SSZ test scripts cannot use `print()` at module scope. Use `saveAsciiText`
to write output to a file:

```ssz
lib f = <file.ssz>;
saveAsciiText("output_out.txt", result);
```

The `_out.txt` convention is used by the test runner for diff comparison.

### Trailing `|` Separator

When building output strings with repeated `result = result + "|" + val`,
the last line should not append the trailing separator. Compare against the
`.expected` file exactly.

### `0d` Float Literals in Mixed Arithmetic

`0d150` is a float literal (150.0). In `cond` contexts, uint/float mixing is
allowed and behaves as uint wraparound:

```ssz
// In a branch cond — uint/float mixing works
cond -dif > 0d150:    // -dif wraps unsigned, compared to 150.0

// Outside cond — type error
uint x = 10;
float y = 0d10;       // OK
if(x + 0d10 > 0d20){  // ERROR: uint + float
```

### Branch/Cond/Comm Semantics

```
branch {
  cond <expr>:
    <body>
  else:
    <body>
  break;
  comm:
    <body>
}
```

- **cond path** runs its body, then falls through to `comm`
- **else + break** skips `comm`
- **comm** always runs unless an explicit `break` precedes it
- In `cond` context, all conds evaluate with uint wraparound arithmetic

### Unix-Path Ambiguity on Windows

The real project directory is `C:\Projects\ikemen-new-ultra` — the AGENTS.md
example `C:\Projects\ikemen-plus-ultra-static` is stale. Use `pwd -P` or
`cygpath -w "$(pwd)"` to get the actual path.

---

## Testing

### SSZ Regression Suite

Located in `test/ssz/`, run by `test/run_ssz_tests.sh` from gitignored
`test/work/`.

| Test | Library | Coverage |
|---|---|---|
| `md5test` | md5 | RFC 1321 vectors (empty, `"abc"`, fox phrase), streaming == one-shot |
| `arcfourtest` | arcfour | RFC 6229 vectors (`Key`/`Plaintext`, `Secret`/`Attack at dawn`), streaming == one-shot |
| `regextest` | regex | Match groups, no-match, invalid pattern |
| `filetest` | file | Save/load/copy/move/remove, dirs, int write/readAry round trip |
| `basetest` | base64 | `encBase64` of `hello`/`Man`/`Ma`, char↔uint helpers, `decBase64` round-trips (ubyte/short/int) |
| `mathtest` | math | Seeded PRNG sequence (deterministic LCG), sqrt/floor/ceil/isnan/isinf |
| `tabletest` | table | `hash` values matching the original SSZ algorithm |
| `timetest` | time | unixTime range, tickCount monotonic |
| `threadtest` | thread | Sleep duration >= requested |
| `sockettest` | socket | Graceful connect failure, listen |
| `soundtest` | sound | `&Client` calls complete (device-dependent) |
| `ssztest` | ssz | compileString + run a snippet, memMark |
| `tmpltest` | tmpl | Native `_t` generics — scalar params/returns, `_t v=` out-param, `^_t buf=` array, `^_t` return, `long` instantiations |
| `funcreftest` | funcref | Native `ref`/`func` delegates — `ref=` out-params, `ref` values, `func$void(int)` / `func$void(int=)` params |
| `mesdialogtest` | mesdialog | Shared-string round trip, ini write/read, CodePage byte↔string conversions, asciiToLocal, tajuuCheck |
| `sdleventtest` | sdlevent | `&Key` reset/checkDown (dot-qualified `|.sdl.K` enum params), modifier-mask matching |
| `sdleventtimingtest` | sdlevent | Deterministic `event(fps)` timing core — every branch path, uint-wraparound conds, frac carry across frames |

### Running Tests

```bash
# Full suite (auto-builds Debug + install first)
make test

# SSZ suite only
make test-ssz

# C++ smoke test only
make test-file

# Direct (after manual build)
bash test/run_ssz_tests.sh
```

### Test Conventions

- Each test writes output to `<name>_out.txt` via `saveAsciiText`
- Frozen reference in `test/ssz/<name>.expected`
- Runner diffs output vs expected — any difference = failure
- Tests run from `test/work/` (gitignored)
- The exe resolves `lib/x.ssz` relative to its own directory
- Tests require `install/ikemen-debug.exe` and a fresh copy of `ssz_script/lib/`

---

## Conversion Status

### Final Numbers

| Metric | Count |
|---|---|
| Native C++ libraries | 20 (16 conversions + 4 alpha bridges) |
| Test fixtures | 2 (tmpl, funcref) |
| Native C++ source lines | 3,126 (3,003 lib + 123 video) |
| SSZ wrapper lines remaining | 3,334 (template-bound + video wrapper) |
| SSZ regression tests | 17 |
| Fully native libs | 3 (time, shell, thread) |
| Hybrid libs | 15 (native core + thin SSZ wrapper) |
| Converted game scripts | 2 (video.ssz, sound.ssz) |
| Intentionally SSZ-only | 2 (consts, alert) |
| Removed (dead code) | 2 (stack, ogg) |

### Per-Library Status

| # | Library | Native C++ | SSZ Remaining | Type |
|---|---|---|---|---|
| 1 | time | 81 | — | 🟢 Fully native |
| 2 | shell | 122 | — | 🟢 Fully native |
| 3 | thread | 65 | — | 🟢 Fully native |
| 4 | math | 206 | 153 | 🔀 Hybrid |
| 5 | string | 372 | 590 | 🔀 Hybrid |
| 6 | md5 | 354 | 45 | 🔀 Hybrid |
| 7 | arcfour | 183 | 29 | 🔀 Hybrid |
| 8 | file | 262 | 144 | 🔀 Hybrid |
| 9 | regex | 143 | 35 | 🔀 Hybrid |
| 10 | sound | 108 | 41 | 🔀 Hybrid |
| 11 | ssz | 79 | 52 | 🔀 Hybrid |
| 12 | socket | 121 | 73 | 🔀 Hybrid |
| 13 | base64 | 182 | 94 | 🔀 Hybrid |
| 14 | table | 76 | 211 | 🔀 Hybrid |
| 15 | sdlplugin | 120 | 1,037 | 🔀 Hybrid |
| 16 | sdlevent | 158 | 582 | 🔀 Hybrid |
| 17 | lua | 73 | 144 | 🔀 Hybrid |
| 18 | mesdialog | 68 | 111 | 🔀 Hybrid |
| — | consts | — | 29 | ⬜ SSZ-only |
| — | alert | — | 4 | ⬜ SSZ-only |

---

## Game Script Conversion (SSZ → Native C++)

### SSZ-to-C++ Transpiler (`tools/ssz_to_cpp.py`)

A Python transpiler that reads `.ssz` files and generates:
1. A **native C++ library** (`.cpp`) with functions following the plugin ABI
2. A **thin SSZ wrapper** (`.ssz`) that keeps type definitions and delegates
   method bodies to C++

**Control-flow translation:**

| SSZ Pattern | C++ Translation |
|---|---|
| `branch { cond expr: body ... else: body comm: }` | `if/else-if/else` chains |
| `break` (inside branch) | `goto end_B{N};` |
| `break, break;` | inner break + outer break (multi-scope exit) |
| `loop { init; while; do: body continue: post while cond: }` | `{ init; do { body; post; } while(cond); }` |
| `continue` | `continue;` (targets while condition, same as SSZ) |
| Nested branch in loop | nested if/else inside do-while |
| Nested loop in branch | nested do-while inside if/else |

**Usage:**
```bash
/c/Python314/python.exe tools/ssz_to_cpp.py <input.ssz> --lib-name <name> --output-dir <dir>
```

### Game Script Classification

Game scripts in `ssz_script/ssz/` are classified by conversion safety:

| Safety | Scripts | Reason |
|---|---|---|
| ✅ **Converted** | `video.ssz` (56 lines) | Leaf node, no game-script deps beyond `common`. Methods use basic types + static plugin calls only. |
| ✅ **Converted (partial)** | `sound.ssz` (406 lines) | `&Bgm.play/clear/write` and `&Sound.setVol` delegated to `sound_game` native lib. Audio mixing, file parsing, and struct definitions stay in SSZ (use `^&.Wave`, `&.file.File`, typed tables). |
| 🔴 **Not convertible** | `font.ssz` (408), `system.ssz` (426), `share.ssz` (370), `loader.ssz` (283), `debug-script.ssz` (295), `ikemen.ssz` (238) | All methods use SSZ-specific types (struct dereferences, delegates, closures, typed tables). The few arithmetic helpers (e.g. `getCharNo`) are trivially small and not worth the native bridge overhead. |
| 🟡 **Risky** | `bg.ssz` (725) | Leaf structurally, but referenced by statebuilder compiled code or loader pipeline |
| 🔴 **Blocked** | `char.ssz` (7,664), `fight.ssz` (3,577), `statebuilder.ssz` (9,333), `command.ssz` (1,571), `sff.ssz` (1,412), `common.ssz` (1,198), `stage.ssz` (735), `fighting.ssz` (670), `system-script.ssz` (2,402), `script.ssz` (2,215), `trigger-script.ssz` (1,632) | In the `compileString`/`sszc~run()` pipeline; types/functions referenced by runtime-generated SSZ code |

**The hard constraint:** The game's loading pipeline (`loader.ssz`) builds SSZ
code strings via `compileString`/`sszc~run()`. The statebuilder generates
runtime SSZ code that references types/functions from `char.ssz`, `common.ssz`,
`command.ssz`, `sff.ssz`, `sound.ssz`, `table.ssz`, and `math.ssz`. Any module
referenced by dynamically generated code cannot be converted to C++ unless the
statebuilder's code generation is also rewritten.

**Scan result:** A thorough scan of all 21 game scripts found that only
`video.ssz` and `sound.ssz` have method signatures expressible with basic
SSZ types. Every other script uses SSZ-specific types (`&.sff.Sprite`,
`&.tbl.IntTable`, `&.re.Regex`, delegates, closures, struct dereferences)
in all its method signatures. The few arithmetic helpers (e.g. `getCharNo`
in `system.ssz`) are trivially small and not worth the native bridge
overhead. **Game script conversion is effectively complete.**

### Type Resolution Limitation (SSZ-Defined Struct Types)

**The blocker for game script conversion:** `NativeLibFrom` resolves native lib
function signatures against the *importing* module's type table. When a native
library references a type defined in the consuming `.ssz` file (e.g., `&.Rect`
defined in `action.ssz`), `BuildPluginType` fails because the type isn't in the
native lib's resolution scope.

**What works:**
- Basic types: `int`, `uint`, `float`, `bool`, `index`, `short`, `byte`, etc.
- Types from the static plugin registry: `^/char`, `^&.sdl.Rect`, `|.sdl.K`
- Opaque `intptr_t*` for struct field params (no type resolution needed)

**What doesn't work:**
- Types defined in the consuming SSZ script: `^&.Rect` (if `&.Rect` is in the
  same `.ssz` that imports the native lib)
- Struct types with SSZ-specific layout: `&.sff.Anim!&.Frame?`

**Example (action.ssz → action.cpp):**
```ssz
// action.ssz defines &Rect, &Frame, &Action, &DrawnClsn
lib act = <action>;   // tries to resolve Frame_clsn1 signature

public &Frame {
  public ^^&.Rect clsn;
  public ^&.Rect clsn1() { ret .act.Frame_clsn1(`clsn=); }
}
```

```cpp
// action.cpp — BuildPluginType fails on "^&.Rect (^^&.Rect=)"
// because &.Rect is defined in action.ssz, not in the native lib context
static intptr_t SSZ_STDCALL Frame_clsn1(
    PluginUtil* pu, intptr_t* clsn) { ... }
```

The `NativeLibFrom` function in `sourcetree.hpp` (line 5492) calls
`BuildPluginType` which invokes `NativeTypeIDCallback`. This callback resolves
types against `this` (the importing module). For `lib act = <action>;` inside
`action.ssz`, the importing module IS `action.ssz` — but the type `&.Rect` is
being resolved in the native lib's synthesized SourceTree, not the importing
module's. The callback returns "type not found" and `NativeLibFrom` returns
nullptr, causing the SSZ compiler to fall back to file resolution (which also
fails because there's no `lib/action.ssz` file).

**Deep analysis of `NativeTypeID` resolution chain:**

The `NativeTypeID` function (line 5448) resolves `.Rect` through this chain:
1. Sets `pst = root` for dot-qualified paths (the compilation root, e.g.
   `ikemen.ssz`)
2. Calls `pst->GetHensuu("Rect")` — walks from `pst` up to `frp` via the
   parent chain, checking `NameToIdx` at each level
3. At the root level, `NameToIdx("Rect")` returns a NEGATIVE index (the `~i`
   encoding used by `GetChild` for class entries). The `ii >= 0` check in
   `GetHensuuIndex` fails, so `GetHensuu` returns nullptr
4. Falls through to `pst->GetFuncId("Rect")` which calls `frp->GetChild(...)`
   with `fukakutei=true`. This DOES find "Rect" in the root's nametable
   (negative index → `subfunc[~i]`) and returns the class's `funcid`
5. Checks `selftype == CLASS_BLOCK` → true → returns the class ID

So `GetFuncId` CAN find the class. The actual failure point is different:
when `lib act = <action>;` is inside action.ssz (a child module loaded via
`lib act = "action.ssz"` from char.ssz), `NativeLibFrom` is called with
`ctx.state = this` (action.ssz). The `BuildPluginType` call resolves `&.Rect`
via `NativeTypeID(".Rect")`. With `pst = root`, the type IS found through the
`GetFuncId` fallback. However, the DEFERRED resolution (added in `b6b36c2`)
stores the function in `pendingNativeFuncs` and retries after `MakeTree()`
finishes — but by that point the SSZ call site (`.actn.Frame_clsn1(...)`)
has already been compiled with a VOID_TOKEN placeholder type, causing a
"Syntax error" at the call site.

**The root cause is a chicken-and-egg problem:** the native lib import
(`lib actn = <action>;`) happens at line 21, before the struct methods
that call it (lines 34-41). But the deferred resolution only updates the
function's type AFTER the entire file is parsed — too late for the call
sites that reference the function with the placeholder type.

**Workaround:** Use `intptr_t*` for all struct field params in native function
signatures, and return `intptr_t` instead of `^&.Rect`. The SSZ wrapper would
need type-casting at the call site. However, this breaks the SSZ type system
(`intptr_t` return ≠ `^&.Rect` return from the JIT's perspective).

**Impact (RESOLVED in `dfe8975`):** The `NativeTypeID` fix now allows native
lib signatures to reference types defined in the importing module's scope.
When a dot-qualified path (`.Rect`) fails in root's nametable, the fallback
`this->GetFuncId` finds the type in the importing child module. Verified with
`action.cpp` (`&.Rect=` out-param) — game boots clean.

The remaining limitation is **forward-declared native libs**: if `lib act =
<action>;` appears BEFORE the type is defined in the file, `BuildPluginType`
still fails because the type hasn't been parsed yet. The deferred resolution
path handles this case by retrying after `MakeTree()`.

### What Would Unblock Full Game Script Conversion

1. ✅ **Immediate-resolution type lookup** (implemented in `dfe8975`):
   `NativeTypeID` now falls through to `this->GetFuncId` when `pst->GetFuncId`
   fails for dot-qualified paths. Combined with the deferred resolution path
   (`6669c5c`) for forward-declared native libs, all `&.struct` type
   signatures are now supported in native lib signatures.
2. **Pre-declare SSZ struct types** in a shared header that the native lib can
   include (breaking the SSZ↔C++ isolation boundary)
3. **Transpile SSZ struct definitions to C++** and include them in the native lib
   build (full two-way type sharing)

Option 1 is now implemented. The remaining blockers for full game script
conversion are the complexity of individual scripts (delegates, closures,
typed containers) rather than the type resolution infrastructure.

---

## Commit History

| Phase | Commits | What landed |
|---|---|---|
| Static plugin groundwork | `e746623` | Bare-name plugin resolution (`<time>` vs `<dll/time.dll>`) |
| Native registry + pilot | `c48d6f9` | `native_lib.hpp` registry; `time` + `shell` fully native |
| Thread | `b7a896b` | `thread` fully native |
| Math PRNG | `34fdbe6` | `random`/`srand`/`rand` native + module-variable support |
| Math trig | `dd0fd66` | `sin`/`cos`/`tan`/… + `round` family native |
| String | `624ac01` | String plain-function core native; `^/char` returns via heap `Reference` |
| MD5 / Arcfour | `ee010e3`, `fa37a84` | One-shot + streaming struct methods; out-param write-back fix |
| File | `6bf8330` | Module functions + `&File` methods |
| Struct delegation | `d66bf18` | `&Regex`/`&Client`/`&Compiler`/`&Socket` via field-out-param pattern |
| Base64 | `b43e368` | Byte core native, templates delegated |
| Winsock fix | `1093846` | Static-build `WSAStartup` gap |
| Table hash | `4bd7064` | `table.hash` native; template analysis documented |
| Dead code cleanup | `f73a978` | Removed md5 T1–T64 constants |
| Regression suite | `bdd3a1c` | `test/ssz/*` + `test/run_ssz_tests.sh` |
| decBase64 fix | `a644791` | `(_t)` → `(*_t)` cast fix |
| Makefile wiring | `d717a0f` | `make test` / `test-file` / `test-ssz` |
| Enum/Struct native types | `6f49edd` | `TypeNameToTokens` + `|Enum`/`&Struct`; `sdlplugin` converted |
| sdlevent &Key | `e2e8e02` | `&Key` methods native; stateful event loop stays in SSZ |
| Native `_t` | `f6526b2` | JIT call-site type inference for native generic signatures |
| `ref`/`func` delegates | `3075c15` | `TypeNameToTokens` encodes `ref`/`func`; `lua` bridge converted |
| Lua FFI fix | `ced08f4` | `EnableExecute` read-after-flip — unblocks full-game boot |
| Mesdialog bridge | `81b3fca` | `mesdialog.cpp` re-exports static plugin fnptrs; `|CodePage` enum sigs |
| Transpiler + game scripts | `3d966a2`, `b6b36c2` | SSZ-to-C++ transpiler with branch/cond/comm/break; video.ssz first game script converted |
| sound.ssz conversion | `(current)` | `sound_game` native lib: `&Bgm.play/clear/write` + `&Sound.setVol`; font.ssz analyzed and found not convertible |
| Deferred type resolution | `6669c5c` | `NativeLibFrom` defers `BuildPluginType` failures; retries after `MakeTree()` — enables forward-declared native lib types |
| NativeTypeID fallback | `dfe8975` | `NativeTypeID` falls through to `this->GetFuncId` when dot-qualified path fails in root — unblocks `&.struct` types from child modules |
| sdlevent timing core | `d105f38` | `event(fps)` timing branch native; deterministic with `now` param |
| Dead file cleanup | `cb82864` | Removed `.bak` files and commented-out `ogg.ssz` import |
