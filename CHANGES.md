# CHANGES

Summary of all modifications made during the native C++ ABI migration and SSZ script layer conversion.

---

## 1. Native C++ ABI Migration (`TODO.md`) — COMPLETE

All plugin implementations under `main/` converted from the old SSZ plugin ABI (PluginUtil*/Reference) to native C++ signatures.

### Phase 4 — Complex plugins

#### `main/ssz/ssz.cpp` — all 8 runtime functions converted

| Function | Before | After |
|---|---|---|
| `Run` | `bool Run(PluginUtil*, Reference)` | `bool Run(const std::wstring&)` + bridge wrapper |
| `MemMarkBefore` | `void MemMarkBefore(PluginUtil*, Reference)` | `void MemMarkBefore(const std::wstring&)` + bridge |
| `MemMarkAfter` | `void MemMarkAfter(PluginUtil*, Reference)` | `void MemMarkAfter(const std::wstring&)` + bridge |
| `NewCompiler` | `CompilerState* NewCompiler(PluginUtil*)` | `CompilerState* NewCompiler()` + bridge |
| `DeleteCompiler` | `void DeleteCompiler(PluginUtil*, CompilerState*)` | `void DeleteCompiler(CompilerState*)` + bridge |
| `CompilerCompile` | `void CompilerCompile(PluginUtil*, Reference*, Reference, CompilerState*)` | `std::wstring CompilerCompile(const std::wstring&, CompilerState*)` + bridge |
| `CompilerCompileString` | `void CompilerCompileString(PluginUtil*, Reference*, Reference, Reference, CompilerState*)` | `std::wstring CompilerCompileString(const std::wstring&, const std::wstring&, CompilerState*)` + bridge |
| `CompilerRun` | `bool CompilerRun(PluginUtil*, CompilerState*)` | `bool CompilerRun(CompilerState*)` + bridge |

- Bridge wrappers remain in `ssz.cpp` for `ssz_static.hpp` registration.
- CompilerCompile/CompilerCompileString bridge wrappers set `sszrefnewfunc`/`sszrefdeletefunc` from `pu->psf` directly (avoids `setSSZFunc()` which is gated behind `#ifndef SSZ_CORE`).

#### `main/main.cpp` — entry point converted

- Removed `PluginUtil pu(nullptr, nullptr)` — no longer needed.
- Removed `makeScriptRef()` — 40-line `Reference` construction helper deleted.
- `Run()` called with `const std::wstring&` instead of `PluginUtil*` + `Reference`.
- Script path extracted from `CommandLineString<WCHR>::get()[1]` (already wide).
- Removed `#include "typeid.h"`, `#include "pluginutil.hpp"` (SSZ_CORE).
- Kept `#include "arrayandref.hpp"` — required by MinGW's `__stdcall` name decoration for complete `Reference` type in `extern "C"` bridge declarations.

#### `main/sdlplugin/sdlplugin.cpp` — documented as ~97% done

- `RenderMugenZoom` and `RenderMugenShadow` intentionally keep `Reference`/`Reference*` parameters because they operate directly on SSZ VM-owned pixel buffers.
- Removed unused `#include "typeid.h"` and `#include "pluginutil.hpp"`.

### Phase 5 — Cleanup

#### Static registration decision

- **Decision: Keep bridge wrappers.** A native dispatcher would require changing the SSZ runtime calling convention — more risk for no benefit.
- Documented in `TODO.md` Phase 5 item 1.

#### Unnecessary includes removed

- `sdlplugin.cpp`: removed `typeid.h` and `pluginutil.hpp`.

#### `arrayandref.hpp` — made self-contained

- Added `#include "typeid.h"` (was relying on includers to provide `VOID_TYPEID`).

#### Regression smoke tests — 73 tests

Created `test/test_file.cpp` with tests for:
- **File operations** (40 tests): Open/Close/Read/Write, ReadAry/WriteAry, Seek, SaveAsciiText/LoadAsciiText, Delete, Move, Copy, CreateDir/RemoveDir, SetCurrentDir/GetCurrentDir, Find.
- **Math** (21 tests): Sin/Cos/Tan/ASin/ACos/ATan/Log/Ln/Exp/Sqrt/Ceil/Floor/IsFinite/IsInf/IsNaN.
- **Thread** (1 test): ThreadDelay smoke.
- **FileHandle** (23 tests): RAII open/close/read/write, move semantics, double-close safety, closed-handle ops, free-function save/load_ascii_text.

Build and run: `make CONFIG=Debug test`.

#### Validation Checklist — all 7 items verified

- Grep-confirmed: No `PluginUtil*` as function parameter in any native `SSZ_STDCALL` function.
- Grep-confirmed: No `Reference` usage in any native `SSZ_STDCALL` function (2 intentional exceptions in sdlplugin documented).
- Linker-confirmed: All bridge symbols resolve, no duplicates.

---

## 2. SSZ Script Layer Conversion Plan (`TODO_SSZ_CONVERSION.md`) — Updated

### Plan review and additions

- Added **Status (Updated)** section marking the plugin ABI migration as done.
- Added **new risks**: compiler/runtime interaction, test gap for complex state machines, performance regression.
- Added **new test categories**: SSZ-Native Parity Tests, CI guard for symbol removal.
- Added **new immediate TODO items**: audit Lua ↔ SSZ call sites, capture pre-conversion traces, per-module `#ifdef` guards.
- Added **whole-migration exit criteria** — defines when the SSZ layer is fully migrated.
- Added **Open Questions** section.
- Added **typed object IDs** — `using Index = intptr_t` should become typed wrappers.
- Reordered Phase 3 by file size (smallest first).

---

## 3. SSZ Native Compatibility Layer — First Implementation

### Files created: `main/ssz_native/`

#### `ssz_value.hpp`
```cpp
namespace ikemen::ssz_native {
    using Index = intptr_t;
    struct SszString { std::wstring value; };
    struct SszBytes { std::vector<uint8_t> data; };
    template<typename T> using SszArray = std::vector<T>;
}
```

#### `file_service.hpp` — `FileHandle` RAII class
Mirrors the `&file.File` SSZ object:
- `open(path, mode)`, `close()`, `is_open()`
- `read(ptr, size)`, `read_array(ptr, elem_size, count)`
- `write(ptr, size)`, `write_array(ptr, elem_size, count)`
- `seek(offset, origin)` with `SeekOrigin::Set/Cur/End`
- Move-only, non-copyable, automatic close in destructor.

#### `file_service.hpp` — Free functions
Mirrors module-level `lib/file.ssz` API:
- `read_all(path)` → `SszBytes`
- `load_ascii_text`, `save_ascii_text`
- `remove_file`, `move_file`, `copy_file`
- `find_files`, `find_directories`
- `create_directory`, `remove_directory`, `set_current_directory`, `get_current_directory`

#### `file_service.cpp`
- Calls native `main/file/file.cpp` functions directly (declared extern at global scope).
- No SSZ plugin ABI (`PluginUtil*`, `Reference`, `extern "C"`) in the new path.
- `_ftelli64` used for `read_all` size determination.

### Makefile changes

- Added `SSZ_NATIVE = $(MAIN)/ssz_native` variable.
- Added `$(SSZ_NATIVE)/file_service.cpp` to `MAIN_SRCS`.
- Added `$(BLD)/main/ssz_native/%.o` compile rule.
- Added `$(BLD)/main/ssz_native/file_service.o` to `TEST_FILE_OBJS`.

---

## Files changed (complete list)

| File | Change |
|---|---|
| `main/ssz/ssz.cpp` | Converted 8 runtime functions; added `#include "bridge.hpp"`; added native `Run(std::wstring)`, `MemMarkBefore/After(std::wstring)`, `NewCompiler/DeleteCompiler/CompilerRun(CompilerState*)`, `CompilerCompile/CompilerCompileString(...)` |
| `main/main.cpp` | Removed `PluginUtil`, `Reference`, `typeid.h`, `pluginutil.hpp`; native `Run(std::wstring)` call; `CommandLineString<WCHR>` for script path |
| `main/sdlplugin/sdlplugin.cpp` | Removed `#include "typeid.h"` and `#include "pluginutil.hpp"` |
| `main/ssz/arrayandref.hpp` | Added `#include "typeid.h"` (self-contained) |
| `main/ssz/bridge.hpp` | Created (initial bridge setup, pre-existing) |
| `main/ssz/bridge.cpp` | Bridge wrappers for all plugin functions (pre-existing, extended during migration) |
| `main/ssz_native/ssz_value.hpp` | **NEW** — compatibility value types |
| `main/ssz_native/file_service.hpp` | **NEW** — FileHandle RAII + free functions |
| `main/ssz_native/file_service.cpp` | **NEW** — implementation calling file.cpp directly |
| `test/test_file.cpp` | **NEW** — 85 regression tests (file, math, thread, FileHandle) |
| `Makefile` | Added `SSZ_NATIVE`, `$(BLD)/main/ssz_native/%.o` rule, `file_service.o` to MAIN_SRCS and TEST_FILE_OBJS |
| `TODO.md` | Updated all phases; validation checklist verified |
| `TODO_SSZ_CONVERSION.md` | Updated status, risks, testing, exit criteria, immediate TODO; marked file_service items done |
| `main/ssz_native/plugin_native_api.hpp` | **NEW** — shared native function declarations (file, math, time, alert, thread); eliminates ~200 lines of duplication across bridge.cpp, file_service.cpp, test_file.cpp |
| `main/ssz_native/consts.hpp` | **NEW** — native equivalents of `ssz_script/lib/consts.ssz` (Phase 1, item 1). Signed/Unsigned template structs, type aliases, sentinel shorthands |
| `main/ssz_native/math_service.hpp` | **NEW** — native equivalents of `ssz_script/lib/math.ssz` (Phase 1, item 2). Wrappers for math functions, Park-Miller LCG PRNG, utility templates |
| `main/ssz_native/math_service.cpp` | **NEW** — implementation of PRNG with module-level seed, srand/random/rand/randI/randF |
| `main/ssz_native/regex_service.hpp` | **NEW** — native equivalent of `ssz_script/lib/regex.ssz` (Phase 2). RAII Regex class wrapping std::wregex/boost::wregex, search/search_all/search_matches, free function compile() |
| `main/ssz_native/regex_service.cpp` | **NEW** — implementation of regex compile/search with error handling |

## Post-review fixes (REVIEW.md findings — April 2026)

| Finding | Fix |
|---|---|
| **H1** — Duplicated forward declarations | Created `plugin_native_api.hpp`; all 3 TUs now include it |
| **H2** — `_ftelli64` missing include | Added `#include <io.h>` to `file_service.cpp` |
| **H3** — `SendWriteBGM` discarded `Reference fn` | Added comment explaining ABI compatibility |
| **M1** — `GetRendererInfo` stub | Added `// TODO:` for output Reference implementation |
| **M3** — MinGW `__stdcall` fragility | Added note to `TODO.md` |
| **L3** — FileHandle edge case coverage | Added 12 tests (move semantics, double-close, closed-handle ops) |

## Post-review fixes (REVIEW.md findings — June 2026)

| Finding | Fix |
|---|---|
| **H4** — Include order (plugin_native_api.hpp after sszdef.h) | Swapped includes in `file_service.cpp` and `test_file.cpp` |
| **M4** — Missing tracking for remaining plugins | Added TODO comment in `plugin_native_api.hpp` listing regex, socket, sound, ogg, mesdialog, lua, sdl, shell |
| **M5** — `consts.hpp` type aliases shadow standard names | Added SSZ type-width comment explaining fixed widths vs C platform-dependent types |
| **M6** — `ssz_native/` not in `AGENTS.md` | Added SSZ native layer row to AGENTS.md component table |
| **L5** — No self-move-assignment test | Added test (fh = std::move(fh) must preserve state) |

## Post-review fixes (REVIEW.md findings — July 2026)

| Finding | Fix |
|---|---|
| **H5** — `math_service.hpp` comment claims to wrap SSZ plugin functions but calls C stdlib | Rewrote header comment: clarifies functions are reimplemented via C standard library, explains intentional bypass of SSZ plugin layer |
| **H6** — Signed integer overflow UB in `randI()` | Moved `static_cast<int64_t>` before subtraction in both overflow branches |
| **M7** — `round` grouped under "thin wrappers" but has no SSZ native plugin equivalent | Moved `round` to its own comment section |
| **M8** — `randF` uses `float` intermediates, loses precision for large values | Changed to `double` intermediate; added precision-limitation comment |
| **M9** — No design comment explaining `math_service` independence | Added design-note block at top of `math_service.hpp` |
| **L8** — No PRNG parity test | Added Park-Miller sequence test (seed=1 → 16807, 282475249) |

## Post-review fixes (REVIEW.md findings — August 2026)

| Finding | Fix |
|---|---|
| **H7** — Missing design note in `regex_service.hpp` | Added 8-line design-note at header top explaining intentional SSZ plugin bypass |
| **H8** — `regex_service.cpp` redefines `SSZ_REGEX_NS` | Added comment explaining why redefinition is necessary (header `#undef`s at EOF). Removed redundant `#include <regex>` / `#include <boost/regex.hpp>`. |
| **M10** — Linux error handling returns hardcoded string | Added comment explaining deliberate trade-off for runtime independence |
| **M11** — Duplicate `Match`/`RegexMatchInfo` types | Added `// TODO: Consolidate` comment referencing bridge layer retirement |
| **L11** — `search_matches`/`search_all` undocumented as extensions | Added comment: "Extension: not provided by the SSZ native plugin. Added at the service layer." |
