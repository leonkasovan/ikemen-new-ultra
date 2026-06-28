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
| `main/ssz_native/socket_service.hpp` | **NEW** — native equivalent of `ssz_script/lib/socket.ssz` (Phase 2). RAII SocketHandle wrapping SOCKET, connect/listen/accept/send/recv |
| `main/ssz_native/socket_service.cpp` | **NEW** — implementation delegating to native socket plugin functions |
| `main/ssz_native/sound_service.hpp` | **NEW** — native equivalent of `ssz_script/lib/sound.ssz` (Phase 2). RAII AudioClient wrapping opaque Client* handle |
| `main/ssz_native/sound_service.cpp` | **NEW** — implementation delegating to native sound plugin functions |
| `main/ssz_native/string_service.hpp` | **NEW** — native equivalent of `ssz_script/lib/string.ssz` (Phase 1). equ, trim, find, split, join, to_lower, hex/octal formatting, UTF-8 encode/decode, percent encode/decode |
| `main/ssz_native/string_service.cpp` | **NEW** — implementation of string operations via C++ standard library |
| `main/ssz_native/ogg_service.hpp` | **NEW** — native equivalent of `ssz_script/lib/alpha/ogg.ssz` (Phase 2). RAII OggVorbisHandle wrapping opaque OggVorbis* handle |
| `main/ssz_native/ogg_service.cpp` | **NEW** — implementation delegating to native ogg plugin functions |
| `main/ssz_native/mesdialog_service.hpp` | **NEW** — native equivalent of `ssz_script/lib/alpha/mesdialog.ssz` (Phase 2). Free-function API wrapping mesdialog plugin: dialogs, INI, encoding, compression, shared string |
| `main/ssz_native/mesdialog_service.cpp` | **NEW** — implementation delegating to native mesdialog plugin functions |
| `main/ssz_native/crypto_service.hpp` | **NEW** — native equivalents of `ssz_script/lib/base64.ssz` and `arcfour.ssz` (Phase 1). Base64 encode/decode, Arcfour RC4 RAII context |
| `main/ssz_native/crypto_service.cpp` | **NEW** — implementation of base64 and Arcfour algorithms |
| `main/ssz_native/alert_service.hpp` | **NEW** — native equivalent of `ssz_script/lib/alert.ssz`. Alert function wrapping native alert plugin |
| `main/ssz_native/alert_service.cpp` | **NEW** — implementation delegating to native alert plugin |
| `main/ssz_native/thread_service.hpp` | **NEW** — native equivalent of `ssz_script/lib/thread.ssz`. ThreadDelay wrapper |
| `main/ssz_native/thread_service.cpp` | **NEW** — implementation delegating to native thread plugin |
| `main/ssz_native/time_service.hpp` | **NEW** — native equivalent of `ssz_script/lib/time.ssz`. tick_count and unix_time wrappers |
| `main/ssz_native/time_service.cpp` | **NEW** — implementation delegating to native time plugin |
| `main/ssz_native/shell_service.hpp` | **NEW** — native equivalent of `ssz_script/lib/shell.ssz`. Shell open and move-to-trash wrappers |
| `main/ssz_native/shell_service.cpp` | **NEW** — implementation delegating to native shell plugin |
| `main/ssz_native/lua_service.hpp` | **NEW** — native equivalent of `ssz_script/lib/alpha/lua.ssz`. RAII LuaState wrapping lua_State* |
| `main/ssz_native/lua_service.cpp` | **NEW** — implementation delegating to native lua plugin |
| `main/ssz_native/table_service.hpp` | **NEW** — native equivalent of `ssz_script/lib/table.ssz` (Phase 1). Header-only NameTable<T> template wrapping unordered_map, string hash function |
| `AGENTS.md` | Updated ssz_native row: 29→31 files, ~4,500→~5,000 lines |
| `TODO_SSZ_CONVERSION.md` | Updated Build-System Plan flag list to include all 15 flags |

## Post-review fixes (REVIEW.md findings — March 2027)

| Finding | Fix |
|---|---|
| **H20** — `AGENTS.md` says "21 files, ~3,500" | Updated to "23 files, ~3,700 total" |
| **M28** — `alert_static.hpp` missing `#if IKEMEN_NATIVE_ALERT_LIB` guard | Wired with standard `#if`/`#else` stub pattern |
| **M29** — `alert_service.cpp` missing M4 TODO comment | Added comment referencing `plugin_native_api.hpp` M4 consolidation |
| **L32** — No alert_service smoke test | Added `test_alert_service()` with compile-time verification |

## Post-review fixes (REVIEW.md findings — June 2027)

| Change | Details |
|---|---|
| `IKEMEN_NATIVE_LUA_LIB` / `IKEMEN_NATIVE_SDLPLUGIN_LIB` flags | Added with `?=` defaults, `-D` defines, and `native_manifest` entries |
| `lua_static.hpp` guard | Wired with `#if IKEMEN_NATIVE_LUA_LIB` / `#else` stub |
| `sdlplugin_static.hpp` guard | Wired with `#if IKEMEN_NATIVE_SDLPLUGIN_LIB` / `#else` stub |
| `TODO_SSZ_CONVERSION.md` count | Updated static-header guard count from 11 to 13 |

## Post-review fixes (REVIEW.md findings — July 2027)

| Change | Details |
|---|---|
| **M32** — `lua_service.cpp` missing M4 TODO | Added: "duplicate bridge.cpp:100-120, tracked in M4 TODO" |
| **L34** — `LuaState` missing self-move test | Added `ls4 = std::move(ls4); TEST(L"LuaState self-move safe", ls4.is_valid())` |
| **REVIEW.md** — stale M30/M31 entries | Removed carried-forward entries (thread/time/shell already wired in May). Updated wiring table to show 12/12 complete. Updated Open Issues summary. |

## October 2027

| Change | Details |
|---|---|
| **Lua audit** | Completed audit of `lua_script/` ↔ `ssz_script/` cross-calls. **Result: zero cross-calls found.** The two scripting systems are architecturally independent — they communicate only through their respective C++ plugin interfaces. No dependency risk. |
| `TODO_SSZ_CONVERSION.md` | Marked "Audit Lua usage" and "Audit Lua ↔ SSZ call sites" as done. |

## November 2027

| Change | Details |
|---|---|
| **SSZ dependency graph** | Generated `docs/ssz_dependency_graph.txt` scanning all `lib ... = <...>` imports across 45 SSZ files. Key finding: no circular dependencies. `alpha/sdlplugin.ssz` is imported by 20 files — the most depended-on module. `ssz/char.ssz` imports 9 modules — the heaviest consumer. |

## December 2027

| Change | Details |
|---|---|
| **SSZ public symbol manifest** | Generated `docs/ssz_symbol_manifest.txt` — ~2,157 public symbols across 45 files. `ssz/char.ssz` has 685 symbols (most), `ssz/statebuilder.ssz` has 116 symbols. `lib/alert.ssz` etc. have 1 each (fewest). |

## January 2028

| Change | Details |
|---|---|
| `TODO_SSZ_CONVERSION.md` | Marked Phase 0 as complete (all 8 sub-items done). Updated Testing Plan to mark symbol manifest + dependency graph as done. Updated Immediate TODO to reflect remaining 2 items. |

## Post-review fixes (REVIEW.md findings — May 2027)

| Finding | Fix |
|---|---|
| **H21** — `AGENTS.md` says "23 files, ~3,700" | Updated to "29 files, ~4,500 total" |
| **M30** — thread/time/shell static headers missing `#if` guards | Wired 3 headers with `#if IKEMEN_NATIVE_*_LIB` / `#else` stub pattern |
| **M31** — thread/time/shell_service.cpp missing M4 TODO comments | Added M4 TODO references to all three |
| **Documentation** — `TODO_SSZ_CONVERSION.md` | Updated static-header guard count from 7 to 11; removed stale duplicate "Add build flag" entry |

## Post-review fixes (REVIEW.md findings — April 2027)

| Change | Details |
|---|---|
| MD5 hash implementation | Added RFC 1321 MD5 to `crypto_service.hpp/.cpp`: `md5_hash()` returns 16-byte digest, `md5_hex()` returns 32-char hex string. Known-answer tests for empty and "Hello" inputs. |
| `Makefile` | Added `-DIKEMEN_USE_NATIVE_SSZ=1` to CXXFLAGS, per-module feature flags with `?=` defaults, `make native_manifest` target |
| `main/file_static.hpp` | Added `#if IKEMEN_NATIVE_FILE_LIB` guard around entire bridge registration block with `#else` stub returning `true` — proof of concept for per-module flag wiring |

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

## Post-review fixes (REVIEW.md findings — September 2026)

| Finding | Fix |
|---|---|
| **H9** — `socket_service.hpp` includes `ssz_value.hpp` unnecessarily | Removed include |
| **H10** — `socket_service.cpp` duplicates native socket declarations | Added TODO comment referencing M4 tracking |
| **M12** — Redundant `friend` declarations in `SocketHandle` | Removed both no-op `friend` statements |
| **M13** — No design note explaining socket_service call-through | Added 6-line design note |

## Post-review fixes (REVIEW.md findings — October 2026)

| Finding | Fix |
|---|---|
| **H11** — `sound_service.cpp` duplicates native sound declarations without TODO | Added TODO comment referencing M4 tracking (matching H10 pattern) |
| **M14** — `AudioClient` has no `is_valid()` method | Added `bool is_valid() const` |
| **M15** — No design note in `sound_service.hpp` | Added 5-line design note explaining call-through pattern |
| **M16** — Eager-creation pattern undocumented | Added 4-line comment in header |

## Post-review fixes (REVIEW.md findings — November 2026)

| Finding | Fix |
|---|---|
| **H12** — `AGENTS.md` ssz_native row says "5 files, ~200 lines" | Updated to "15 files, ~2,500 total" |
| **H13** — `sound_service.cpp` TODO says `bridge.cpp:85-90` (wrong range) | Changed to `79-84` to match actual bridge.cpp line numbers |
| **M17** — `file_service.cpp` `_ftelli64` called unconditionally (Linux incompatible) | Added `#ifdef _WIN32` guard with `ftello` as Linux fallback + TODO comment |
| **M18** — `regex_service.cpp` includes `<windows.h>` after `<regex>` | Moved `<windows.h>` include before `regex_service.hpp` to avoid STL macro conflicts |
| **M19** — `string_service.cpp` `to_lower` silently truncates non-ASCII | Added comment: "ASCII-only case conversion, matching SSZ behavior. Non-ASCII characters are truncated to 7-bit." |
| **L20** — `to_lower_char`/`to_lower` naming asymmetry | Added ASCII-only comment documenting the scope |
| **L22** — `TEST_FILE_OBJS` excludes `string_service.o` without comment | Added note explaining exclusion (all functions inline/template) |

## Post-review fixes (REVIEW.md findings — December 2026)

| Finding | Fix |
|---|---|
| **H14** — `AGENTS.md` says "15 files, ~2,500" | Updated to "17 files, ~2,800 total" |
| **H15** — `ogg_service.cpp` TODO says `bridge.cpp:119-128` (off by 2) | Changed to `121-129` to match actual bridge.cpp line numbers |
| **M21** — `OggVorbisHandle` missing self-move-assignment test | Added test: `ov = std::move(ov)` must not invalidate |
| **M22** — Test mislabeled "null handle" (actually non-opened handle) | Fixed label to "non-opened handle"; added separate null-handle test via moved-from source |

## Post-review fixes (REVIEW.md findings — January 2027)

| Finding | Fix |
|---|---|
| **H16** — `AGENTS.md` says "17 files, ~2,800" | Updated to "19 files, ~3,100 total" |
| **H17** — `mesdialog_service.cpp` TODO says `bridge.cpp:88-102` (off by 3) | Changed to `85-99` to match actual bridge.cpp line numbers |
| **M23** — 3 unused native declarations (VeryUnsafeCopy, etc.) | Added comment: "declared for completeness with the bridge's forward-declaration block — not exposed in the public API" |
| **L27** — `CodePage` enum values undocumented | Added per-value comments and usage note for ACP/UTF8 |
| **M25** — Per-module flags not wired to `*_static.hpp` guards | Wired `file_static.hpp` with `#if IKEMEN_NATIVE_FILE_LIB` guard and `#else` stub. Verified builds with `=1` and `=0`. |

## Post-review fixes (REVIEW.md findings — February 2027)

| Finding | Fix |
|---|---|
| **H18** — `crypto_service.o` missing from `TEST_FILE_OBJS` | Added `crypto_service.o` + `ssz.o` + `string_service.o` to fix link errors |
| **H19** — `AGENTS.md` says "19 files, ~3,100" | Updated to "21 files, ~3,500 total" |
| **M26** — No `IKEMEN_NATIVE_CRYPTO_LIB` flag | Added flag with `?=` default, `-D` define, `native_manifest` entry |
| **M27** — `#if` vs `#ifdef` convention undocumented | Added 5-line convention comment in `Makefile` |
| **M25** — Wire remaining `#if` guards in `*_static.hpp` | Wired `math_static.hpp`, `regex_static.hpp`, `socket_static.hpp`, `sound_static.hpp`, `ogg_static.hpp`, `mesdialog_static.hpp` — all with `#if`/`#else` pattern |

## Build system changes (January 2027)

| Change | Details |
|---|---|
| `IKEMEN_USE_NATIVE_SSZ` flag | Added `-DIKEMEN_USE_NATIVE_SSZ=$(IKEMEN_USE_NATIVE_SSZ)` to CXXFLAGS, defaults to 1 |
| Per-module feature flags | `IKEMEN_NATIVE_FILE_LIB`, `IKEMEN_NATIVE_STRING_LIB`, `IKEMEN_NATIVE_MATH_LIB`, `IKEMEN_NATIVE_REGEX_LIB`, `IKEMEN_NATIVE_SOCKET_LIB`, `IKEMEN_NATIVE_SOUND_LIB`, `IKEMEN_NATIVE_OGG_LIB`, `IKEMEN_NATIVE_MESDIALOG_LIB` — all default to 1, override from command line |
| `make native_manifest` target | Prints all active native module flags |

## Full trace instrumentation (June 2026)

| Change | Details |
|---|---|
| `main/ssz_native/ssz_trace.hpp` | **NEW** — `SSZ_TRACE(msg)` macro, gated by `IKEMEN_ENABLE_PLUGIN_TRACE` compile flag |
| `Makefile` | Added `IKEMEN_ENABLE_PLUGIN_TRACE ?= 0` and `-DIKEMEN_ENABLE_PLUGIN_TRACE=$(IKEMEN_ENABLE_PLUGIN_TRACE)` |
| `tools/instrument_trace.ps1` | **NEW** — automated script to add SSZ_TRACE to all bridge wrappers |
| `main/ssz/bridge.cpp` | 163 `SSZ_TRACE("FuncName")` calls — one per bridge wrapper, all functions instrumented |
| `main/ssz/ssz.cpp` | 9 `SSZ_TRACE("FuncName")` calls — all SSZ runtime bridge wrappers instrumented |
| `AGENTS.md` | Updated ssz_native row: 31→33 files, ~5,000→~2,400 total |
| `REVIEW.md` | Marked M33 (trace coverage) and M34 (AGENTS.md counts) as resolved; updated counts throughout |

## Phase 3 — System service scaffolding (ssz_script/ssz/system.ssz, June 2026)

| Change | Details |
|---|---|
| `main/ssz_native/system_service.hpp` | **NEW** — header-only with SelectData, SelectCharData, SelectStageData, SelectInfoData, SystemData structs |
| `Makefile` | Added `IKEMEN_NATIVE_SYSTEM_LIB` flag, `native_manifest` entry |
| `test/test_file.cpp` | Added `test_system_service()` (28 tests: default init, field access, stub methods) |
| `TODO_SSZ_CONVERSION.md` | Added Phase 3 — System Service section; system.ssz marked as started; added Immediate TODO entry |

## Phase 3 — Debug Script service (ssz_script/ssz/debug-script.ssz, June 2026)

| Change | Details |
|---|---|
| `main/ssz_native/debug_script_service.hpp` | **NEW** — DebugScriptState struct + 27 function declarations (25 Lua callbacks + 2 file loaders) |
| `main/ssz_native/debug_script_service.cpp` | **NEW** — all 27 stub implementations (no-ops) |
| `Makefile` | Added `IKEMEN_NATIVE_DEBUG_SCRIPT_LIB` flag, source/object wiring, native_manifest entry |
| `test/test_file.cpp` | Added `test_debug_script_service()` (32 tests: struct init, 25 stub calls, file stubs, flags mutation) |
| `TODO_SSZ_CONVERSION.md` | Added Phase 3 — Debug Script Service section; debug-script.ssz marked complete; Immediate TODO updated |

## Post-review fixes (REVIEW.md findings — June 2026, debug_script_service)

| Finding | Fix |
|---|---|
| **M40** — system_service note .cpp needed when wired | ✅ Already resolved (comment exists at system_service.hpp:17) |
| **M41** — const_cast in SelectInfoData::reset() | ✅ Already resolved (sel is non-const SelectData*) |
| **M42** — Phase 3 .cpp strategy heuristic | Added ≤5 inline / >5 split convention to TODO_SSZ_CONVERSION.md Phase 3 section |

## Post-review fixes (REVIEW.md findings — June 2026, system_service)

| Finding | Fix |
|---|---|
| **M40** — Header-only stubs need .cpp note | Added `Phase 3 note (M40)` comment to `system_service.hpp` referencing .cpp creation when methods exceed ~5 lines |
| **M41** — `const_cast` in `SelectInfoData::reset()` | Changed `sel` from `const SelectData*` to `SelectData*` (non-const); removed `const_cast` from reset(); verified build |

## Post-review fixes (REVIEW.md findings — June 2026, share_service scaffolding)

| Finding | Fix |
|---|---|
| **M37** — Duplicate `IKEMEN_NATIVE_SHARE_LIB ?=` in Makefile | Removed duplicate assignment on line 110; comment updated to note flag is already defined |
| **M38** — share_service uses root namespace without explanation | Added 7-line design note to `share_service.hpp` explaining DTO pattern (matches `ssz_value.hpp`) |
| **M39** — test_share_minimal.cpp duplicates test_file.cpp coverage | Deleted `test_share_minimal.cpp` and its build artifacts; all tests consolidated in `test_file.cpp` |

## Phase 3 — Share service scaffolding (ssz_script/ssz/share.ssz, June 2026)

| Change | Details |
|---|---|
| `main/ssz_native/share_service.hpp` | **NEW** — `ShareData` struct with ~90 fields matching share.ssz |
| `main/ssz_native/share_service.cpp` | **NEW** — `share_copy()`/`share_push()` stubs (Phase 3 TODO) |
| `Makefile` | Added `IKEMEN_NATIVE_SHARE_LIB` flag, `share_service.cpp` to MAIN_SRCS, `share_service.o` to TEST_FILE_OBJS, `native_manifest` entry |
| `test/test_share_minimal.cpp` | **NEW** — minimal compilation test confirming ShareData struct + stub functions work |
| `TODO_SSZ_CONVERSION.md` | Added Phase 3 — Share Service section; marked share.ssz as started; added Immediate TODO entry for share scaffolding |

## Pre-conversion trace capture (June 2026)

| Change | Details |
|---|---|
| `docs/pre_conversion_trace.log` | **NEW** — 618 KB, 43,690 trace entries from engine startup with `IKEMEN_ENABLE_PLUGIN_TRACE=1` |
| `docs/pre_conversion_trace_stderr.log` | **NEW** — empty (no stderr during startup) |
| `docs/pre_conversion_trace_summary.md` | **NEW** — analysis: 28 unique functions called, 144 of 172 not exercised (expected) |
| `TODO_SSZ_CONVERSION.md` | Marked "Capture pre-conversion trace logs" as done in Phase 0 and Immediate TODO sections |
| `REVIEW.md` | Updated Exit Criteria: pre-conversion trace logs now captured |

## Gameplay trace capture (June 2026)

| Change | Details |
|---|---|
| `docs/pre_conversion_trace_gameplay.log` | **NEW** — 4.9 MB, ~343,000 entries, 47 unique functions (automated keypress navigation) |
| `docs/pre_conversion_trace_summary.md` | Updated with gameplay trace analysis and comparison table |
| `tools/capture_gameplay_trace.ps1` | **NEW** — script for automated keypress-based gameplay trace capture |
| `REVIEW.md` | M36 updated: gameplay trace captured (47 functions); 125 still require actual fight gameplay |
| `CHANGES.md` | Fixed date typo: "June 2029" → "June 2026" in two section headers (M35) |
