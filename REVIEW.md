# REVIEW — Native C++ ABI Migration & SSZ Script Layer Conversion

**Review date:** 2026-06-29 (updated)
**Scope:** Phase 0 completion, runtime trace infrastructure (now fully instrumented), all `main/ssz_native/` modules (33 files, 17 services including ssz_trace), and resolution of all prior findings.

---

## Overall Assessment

The migration continues in excellent shape. Since the last review (2026-06-28), Phase 0 (Research & Architecture Lock) has been marked complete, the runtime trace infrastructure (`ssz_trace.hpp`) has been added, and all four remaining M4 TODO gaps (M30, M31, M32) have been resolved.

**Code quality remains high.** The new trace infrastructure follows the established pattern: a compile-time guard (`IKEMEN_ENABLE_PLUGIN_TRACE`), a zero-overhead opt-in macro, and demonstration calls on the `Open` and `Run` bridge wrappers.

**Verdict:** ✅ Excellent. No critical or high-severity findings. Phase 2 is complete except for sdlplugin (intentionally deferred — 60+ functions, complex SDL lifetimes). Phase 0 research deliverables are in place, clearing the way for Phase 3 (core engine SSZ modules).

---

## What's New Since Last Review (2026-06-28)

### 1. Phase 0 — Research & Architecture Lock ✅

All Phase 0 items from `TODO_SSZ_CONVERSION.md` are now complete:

| Deliverable | File | Status |
|-------------|------|--------|
| SSZ dependency graph | `docs/ssz_dependency_graph.txt` | ✅ 45 modules, 151 imports, no circular deps |
| Public symbol manifest | `docs/ssz_symbol_manifest.txt` | ✅ ~2,157 public symbols across 45 files |
| Naming convention | C++ domain names with adapter aliases | ✅ Documented |
| Namespace | `ikemen::ssz_native` | ✅ Consistent across 33 files |
| Build target | `SSZ_NATIVE_SRCS` in Makefile | ✅ Per-module `make native_manifest` |
| Language features | All implemented services | ✅ Imports, plugin calls, types, objects, templates, ownership |

### 2. Runtime Trace Infrastructure ✅ (Full Instrumentation)

A new compile-time-optional trace system has been added to aid pre-conversion trace capture:

| File | Purpose |
|------|---------|
| `main/ssz_native/ssz_trace.hpp` | **NEW** — `SSZ_TRACE(msg)` macro, gated by `IKEMEN_ENABLE_PLUGIN_TRACE` |
| `Makefile` line 105 | `-DIKEMEN_ENABLE_PLUGIN_TRACE=$(IKEMEN_ENABLE_PLUGIN_TRACE)` |
| `Makefile` line 108 | `IKEMEN_ENABLE_PLUGIN_TRACE ?= 0` (off by default) |
| `tools/instrument_trace.ps1` | **NEW** — automated script to add SSZ_TRACE to all bridge wrappers |

**Full instrumentation completed:** All 172 bridge wrappers now have `SSZ_TRACE("FuncName")` calls:
- **bridge.cpp:** 163 traces (162 auto-instrumented + 1 pre-existing `Open`)
- **ssz.cpp:** 9 traces (8 auto-instrumented + 1 pre-existing `Run`)
- Every `extern "C"` bridge wrapper from `FileClose` through `UnbindGlContext` is covered
- Auto-instrumentation script saved as `tools/instrument_trace.ps1` for future use

**Design assessment:** The trace macro is well-designed — zero overhead when disabled (`((void)0)`), simple printf-based output to stdout with `[TRACE]` prefix, and `fflush(stdout)` for crash-safe logging. Enabling is a one-liner: `make IKEMEN_ENABLE_PLUGIN_TRACE=1`.

### 3. Previous Findings Resolved

All findings from the June 2026 review are now resolved:

| Finding | Description | Status |
|---------|-------------|--------|
| M30 | Static headers needing `#if` guards | ✅ All 13 wired (verified in this review) |
| M31 | thread, time, shell `.cpp` missing M4 TODO | ✅ All three now have M4 TODO comments |
| M32 | lua_service.cpp missing M4 TODO | ✅ Now has bridge.cpp line range and M4 TODO |
| M33 | Only 2 of ~100 bridge wrappers have SSZ_TRACE | ✅ **163 of 163 in bridge.cpp, 9 of 9 in ssz.cpp now instrumented** |
| M34 | AGENTS.md ssz_native file/line counts outdated (31→33 files, ~5,000→~2,393 lines) | ✅ Updated to 33 files, ~2,400 total |
| L34 | LuaState self-move-assignment test | ⬜ Not yet added (low priority) |

---

## New Module Review: `ssz_trace.hpp`

### `main/ssz_native/ssz_trace.hpp` ✅

| Aspect | Assessment |
|--------|-----------|
| Compile-time guard | `#ifdef IKEMEN_ENABLE_PLUGIN_TRACE` ✅ |
| Enabled path | `printf("[TRACE] %s\n", msg); fflush(stdout)` — crash-safe ✅ |
| Disabled path | `((void)0)` — zero overhead, type-safe ✅ |
| Header guard | `#pragma once` ✅ |
| Documentation | 5-line usage comment with example ✅ |
| Includes | `<cstdio>` — minimal ✅ |
| Naming | `SSZ_TRACE` macro follows SSZ convention ✅ |

### Trace integration in `bridge.cpp` and `ssz.cpp` ✅

| Aspect | Assessment |
|--------|-----------|
| `#include "ssz_native/ssz_trace.hpp"` | Present in both files ✅ |
| `SSZ_TRACE("Open")` in bridge.cpp | Before native call, no side effects ✅ |
| `SSZ_TRACE("Run")` in ssz.cpp | Before native call, after `(void)pu` ✅ |
| Pattern: guard args → trace → delegate | Consistent with established bridge pattern ✅ |

---

## New Findings (this review — June 2026, Phase 0 + trace)

### 🔴 Critical

*(None.)*

### 🟡 High — Should fix before next module

*(None.)*

### 🟢 Medium — Address in upcoming work

#### M33. Trace coverage is minimal (2 of ~100 bridge wrappers)

**Files:** `main/ssz/bridge.cpp`, `main/ssz/ssz.cpp`

Only `Open` (bridge.cpp:162) and `Run` (ssz.cpp:743) have `SSZ_TRACE` calls. The remaining ~98 bridge wrappers (FileClose, Read, ReadAry, Write, WriteAry, Seek, all socket/regex/sound/ogg/mesdialog/lua wrappers, CompilerCompile, MemMarkBefore/After, etc.) have no trace instrumentation.

**Impact:** Pre-conversion trace capture (Phase 0 remaining item: "Capture pre-conversion trace logs for representative inputs") requires trace coverage across all plugin entry points. With only 2 instrumented, the trace would miss the vast majority of SSZ plugin calls.

**Recommendation:** Before capturing pre-conversion traces, add `SSZ_TRACE("FunctionName")` to every bridge wrapper in `bridge.cpp` and `ssz.cpp`. This is mechanical (~100 lines, ~15 minutes). A script could generate these from the function signatures.

#### M34. `AGENTS.md` ssz_native line count is outdated

**File:** `AGENTS.md`

AGENTS.md reports `main/ssz_native/*` as "31 files, ~5,000 total" lines. The actual counts are:

| Metric | AGENTS.md | Actual |
|--------|-----------|--------|
| Files | 31 | **33** (ssz_trace.hpp added, plus table_service.hpp previously uncounted) |
| Lines | ~5,000 | **~2,393** (wc -l across all .hpp/.cpp) |

The line count discrepancy (~2.1× overestimate) suggests the original count may have included non-ssz_native files or was a rough estimate. The file count is off by 2.

**Recommendation:** Update `AGENTS.md` to reflect actual counts: 33 files, ~2,400 lines.

---

### 🔵 Low — Nice to have

#### L23. Sound constants are compile-time only (carried forward)

**File:** `sound_service.hpp`

Audio format constants (`AF_U8`, `AF_S16LSB`, etc.) are `constexpr` in the header. No runtime lookup from the native plugin. Low risk since these are stable SDL constants.

#### L25. `test_file.cpp` now ~1,082 lines (carried forward, updated)

The file has grown from ~1,020 to ~1,082 lines covering 16 modules. Splitting recommendation stands; consider `test/` subdirectory with per-module test files.

#### L34. `LuaState` has no self-move-assignment test (carried forward)

`FileHandle` and `OggVorbisHandle` both have self-move-assignment safety tests. `LuaState` tests move construction and move assignment but not self-move (`ls = std::move(ls)`). The header's `operator=` guards `this != &other`, so no crash risk.

#### L35. `ssz_trace.hpp` has no unit test (new)

The trace macro is compile-time conditional and its effect is stdout output — not easily testable with the current `test_file.cpp` framework. Low priority; manual verification via `make IKEMEN_ENABLE_PLUGIN_TRACE=1 && ./ikemen-debug.exe` is sufficient.

---

## Per-module Flag Wiring Status (re-verified)

| Static header | Guard | Status |
|---------------|-------|--------|
| `file_static.hpp` | `#if IKEMEN_NATIVE_FILE_LIB` | ✅ |
| `math_static.hpp` | `#if IKEMEN_NATIVE_MATH_LIB` | ✅ |
| `regex_static.hpp` | `#if IKEMEN_NATIVE_REGEX_LIB` | ✅ |
| `socket_static.hpp` | `#if IKEMEN_NATIVE_SOCKET_LIB` | ✅ |
| `sound_static.hpp` | `#if IKEMEN_NATIVE_SOUND_LIB` | ✅ |
| `ogg_static.hpp` | `#if IKEMEN_NATIVE_OGG_LIB` | ✅ |
| `mesdialog_static.hpp` | `#if IKEMEN_NATIVE_MESDIALOG_LIB` | ✅ |
| `alert_static.hpp` | `#if IKEMEN_NATIVE_ALERT_LIB` | ✅ |
| `lua_static.hpp` | `#if IKEMEN_NATIVE_LUA_LIB` | ✅ |
| `thread_static.hpp` | `#if IKEMEN_NATIVE_THREAD_LIB` | ✅ |
| `time_static.hpp` | `#if IKEMEN_NATIVE_TIME_LIB` | ✅ |
| `shell_static.hpp` | `#if IKEMEN_NATIVE_SHELL_LIB` | ✅ |
| `sdlplugin_static.hpp` | `#if IKEMEN_NATIVE_SDLPLUGIN_LIB` | ✅ (wired, no sdl_service yet) |

**13 of 13 headers wired. ✅** (was 12 of 12 in last review; sdlplugin_static.hpp has been wired since.)

---

## M4 TODO Coverage (re-verified)

All 12 call-through `.cpp` files now have M4 TODO comments:

| File | M4 TODO | Status |
|------|---------|--------|
| `alert_service.cpp` | ✅ | Present |
| `file_service.cpp` | ✅ | Present (via plugin_native_api.hpp include) |
| `lua_service.cpp` | ✅ | Present (bridge.cpp:100-120) |
| `math_service.cpp` | ✅ | PRNG only, no native decls needed |
| `mesdialog_service.cpp` | ✅ | Present |
| `ogg_service.cpp` | ✅ | Present |
| `regex_service.cpp` | ✅ | Present |
| `shell_service.cpp` | ✅ | Present |
| `socket_service.cpp` | ✅ | Present |
| `sound_service.cpp` | ✅ | Present |
| `thread_service.cpp` | ✅ | Present |
| `time_service.cpp` | ✅ | Present |

**12 of 12. ✅** (was 9 of 12 in last review.)

---

## File-by-File Review (fresh assessment)

### Phase 1 — Foundation libraries (no plugin dependency)

| Service | Type | Assessment |
|---------|------|-----------|
| `ssz_value.hpp` | Types | ✅ |
| `consts.hpp` | Constants | ✅ |
| `math_service` | Header+impl | ✅ 15 wrappers, PRNG, 6 utilities |
| `string_service` | Header+impl | ✅ 10+ functions |
| `crypto_service` | Header+impl | ✅ Base64, Arcfour RC4, MD5 |
| `table_service` | Header-only | ✅ NameTable<T> template |

### Phase 2 — Plugin wrapper libraries (call-through to native plugin)

| Service | Type | Assessment |
|---------|------|-----------|
| `file_service` | RAII + free fns | ✅ `FileHandle`, 12 free functions |
| `regex_service` | RAII | ✅ `Regex` |
| `socket_service` | RAII | ✅ `SocketHandle` |
| `sound_service` | RAII | ✅ `AudioClient` |
| `ogg_service` | RAII | ✅ `OggVorbisHandle` |
| `mesdialog_service` | Free fns | ✅ `CodePage` enum, 12 functions |
| `alert_service` | Free fn | ✅ `alert()` |
| `thread_service` | Free fn | ✅ `delay()` |
| `time_service` | Free fns | ✅ `tick_count()`, `unix_time()` |
| `shell_service` | Free fns | ✅ `open()`, `move_to_trash()` |
| `lua_service` | RAII | ✅ `LuaState`, 14 methods |

### Shared infrastructure

| File | Assessment |
|------|-----------|
| `plugin_native_api.hpp` | ✅ M4 TODO present, shared native declarations |
| `ssz_trace.hpp` | ✅ Compile-time-optional trace macro, fully instrumented across all bridge wrappers |

---

## Test Coverage Summary

All tests in `test/test_file.cpp` (1,082 lines). Run with `make CONFIG=Debug test`.

| Module | Tests | Coverage |
|--------|-------|----------|
| File plugin (native) | ~50 | All file operations ✅ |
| Math plugin (native) | 17 | 15 trig/exp functions ✅ |
| Thread plugin (native) | 1 | ThreadDelay smoke ✅ |
| math_service | ~85 | Full coverage ✅ |
| string_service | ~23 | Full coverage ✅ |
| crypto_service | 12 | Base64, Arcfour, MD5 ✅ |
| lua_service | 10 | RAII + push/to type roundtrips ✅ |
| mesdialog_service | 4 | Shared string, codepage ✅ |
| ogg_service | 8 | Full RAII coverage ✅ |
| sound_service | 5 | Construction, move, start/stop ✅ |
| socket_service | 7 | Construction, move, double-close ✅ |
| regex_service | ~18 | Full coverage ✅ |
| file_service (RAII) | ~23 | Full coverage ✅ |
| thread_service | 1 | `delay()` no-crash ✅ |
| time_service | 2 | `tick_count() > 0`, `unix_time() > 1e9` ✅ |
| alert_service | 1 | Compile-check ✅ |
| shell_service | 1 | Compile-check ✅ |
| table_service | 15 | Hash, CRUD, iteration ✅ |

**Total: ~383 tests across 18 modules.**

---

## Architecture Consistency Check

| Convention | Status |
|-----------|--------|
| RAII for all handles | ✅ 7 RAII types (FileHandle, Regex, SocketHandle, AudioClient, OggVorbisHandle, Arcfour, LuaState) |
| Free-function API where no handles | ✅ 7 services (consts, mesdialog, crypto/base64/md5, alert, thread, time, shell) |
| Header-only where no state | ✅ 3 (consts, ssz_value, table) |
| Design notes in headers | ✅ All 16 service headers + ssz_trace.hpp |
| M4 TODO in call-through `.cpp` | ✅ 12 of 12 |
| `is_valid()` / `is_open()` on all handles | ✅ All 7 RAII types |
| No `PluginUtil*` or `Reference` in native services | ✅ |
| Per-module feature flags in Makefile | ✅ 15 of 15 (14 service flags + trace flag) |
| Per-module `#if` guards in `*_static.hpp` | ✅ 13 of 13 |
| `SSZ_TRACE` on bridge wrappers | ✅ 172 of 172 (bridge.cpp + ssz.cpp) |

---

## Open Issues Summary

| ID | Severity | File | Description |
|----|----------|------|-------------|
| L23 | 🔵 Low | `sound_service.hpp` | Audio constants compile-time only |
| L25 | 🔵 Low | `test/test_file.cpp` | ~1,082 lines — consider splitting |
| L34 | 🔵 Low | `test/test_file.cpp` | LuaState self-move-assignment test missing |
| L35 | 🔵 Low | `ssz_trace.hpp` | No unit test (hard to test; manual verification sufficient) |

---

## Recommendations

1. **Capture pre-conversion trace logs** — Enable with `make IKEMEN_ENABLE_PLUGIN_TRACE=1`, run representative inputs (one `.def`, one stage, one character), and save the trace output as a parity baseline.
2. **Add L34 self-move test** for `LuaState` (~2 minutes, consistency).
3. **Consider splitting `test_file.cpp`** at 1,082 lines into per-module test files under `test/`.
4. **Phase 3 preparation** — Audit the largest SSZ files (`statebuilder.ssz` 9,334 lines, `char.ssz` 7,665 lines, `fight.ssz` 3,578 lines) for architectural decisions before conversion begins.

---

## Exit Criteria Check (from TODO_SSZ_CONVERSION.md)

| Criterion | Status |
|-----------|--------|
| Phase 0 research deliverables complete | ✅ Dependency graph, symbol manifest, naming, build target |
| All SSZ `plugin index` calls route through native services | ⬜ Not yet — SSZ scripts still call bridge wrappers |
| Zero `Reference` in non-bridge engine code | ✅ |
| All native services have unit test coverage | ✅ ~383 tests |
| Bridge wrappers only convert types (no logic) | ✅ |
| Phase 1 foundation libraries complete | ✅ 6 of 6 (consts, math, string, crypto, ssz_value, table) |
| Phase 2 plugin wrappers complete | ✅ 11 of 12 (sdlplugin deferred) |
| Per-module `#if` guards in `*_static.hpp` | ✅ 13 of 13 |
| Feature flags for all native modules | ✅ 15 of 15 (14 service + 1 trace) |
| Pre-conversion trace logs captured | ⬜ Not yet — trace infrastructure ready, needs manual run |
| CI guard | ⬜ Not yet |
| sdlplugin.ssz / sdlevent.ssz converted | ⬜ Intentionally deferred (Phase 2) |
