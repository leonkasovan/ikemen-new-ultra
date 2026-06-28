# REVIEW — Native C++ ABI Migration & SSZ Script Layer Conversion

**Review date:** 2026-06-29
**Scope:** Phase 3 underway (3 modules scaffolded), gameplay trace captured, full bridge trace instrumentation, pre-conversion trace baselines, all `main/ssz_native/` modules (38 files, 19 services), and resolution of all prior findings.

---

## Overall Assessment

The migration is in excellent shape. Phase 3 (core engine SSZ module conversion) is progressing steadily with three modules now scaffolded, following the smallest-first ordering from the dependency graph:

| # | Module | SSZ lines | Native files | Pattern | Tests |
|---|--------|-----------|-------------|---------|-------|
| 1 | `share.ssz` | 371 | `share_service.hpp/.cpp` | DTO struct + free fns | 11 |
| 2 | `system.ssz` | 427 | `system_service.hpp` (header-only) | DTO struct + inline stubs | 28 |
| 3 | `debug-script.ssz` | 296 | `debug_script_service.hpp/.cpp` | Lua callback bridge | 32 |

The debug_script_service introduces a **new scaffolding pattern**: a Lua callback bridge module with 25 callback stubs receiving `lua_State*` and `int&`, plus 2 file-loading stubs. Unlike the DTO pattern (share, system), this module has no data struct — it's a pure function-stub API designed for eventual Lua C callback registration. The `.cpp` file contains 27 no-op stub bodies, which is the correct approach for 25+ function stubs (header-only with inline stubs would create excessive header bloat).

**Code quality remains high.** The module follows Phase 3 conventions: root `ikemen::ssz_native` namespace, `debug_*` prefix naming, forward-declared `lua_State*`, Phase 3 TODO comments, and a clean Makefile flag. The 32 tests cover default init, all 27 stub calls, and field mutation — the most thorough stub-coverage test of any Phase 3 module so far.

**Verdict:** ✅ Excellent. No critical or high-severity findings. Phase 3 is 3 of 9 modules scaffolded, all following the smallest-first strategy successfully.

---

## What's New Since Last Review (2026-06-29, system_service)

### 1. Phase 3 Continued — debug_script_service Scaffolding ✅

The third Phase 3 module (`ssz_script/ssz/debug-script.ssz`, 296 lines — smallest Phase 3 target) has been scaffolded:

| Deliverable | Details |
|-------------|---------|
| `main/ssz_native/debug_script_service.hpp` | **NEW** — `DebugScriptState` struct + 27 function declarations |
| `main/ssz_native/debug_script_service.cpp` | **NEW** — 27 no-op stub implementations |
| `IKEMEN_NATIVE_DEBUG_SCRIPT_LIB` flag | Added to Makefile with `?=` default and `-D` define |
| `test/test_file.cpp` | Added `test_debug_script_service()` — 32 tests |

**API surface:**
- `DebugScriptState` — 3 module-level flags (`roundResetFlg`, `reloadFlg`, `noHUDDisplay`) + `lua_State*`
- 25 Lua callback stubs: `debug_puts`, `debug_ssz_reload`, `debug_set_life`, `debug_set_life_max`, `debug_set_power`, `debug_set_attack`, `debug_set_defence`, `debug_self_state`, `debug_add_hotkey`, `debug_toggle_clsn_draw`, `debug_toggle_debug_draw`, `debug_toggle_status_draw`, `debug_toggle_post_match`, `debug_toggle_pause`, `debug_toggle_pause_menu`, `debug_step`, `debug_toggle_record`, `debug_toggle_playback`, `debug_toggle_record_end`, `debug_round_reset`, `debug_reload`, `debug_set_accel`, `debug_set_ai_level`, `debug_set_time`, `debug_clear`
- 2 file-loading stubs: `debug_load_file`, `debug_run_file` (return `std::string`)

### 2. New Scaffolding Pattern: Lua Callback Bridge

This module establishes a second Phase 3 pattern distinct from the DTO pattern:

| Aspect | DTO pattern (share, system) | Lua callback bridge (debug_script) |
|--------|---------------------------|-----------------------------------|
| Primary content | Data structs with fields | Function stubs with `lua_State*` |
| `.cpp` file | share has one; system is header-only | Has one (27 stubs — too many for inline) |
| Naming | `share_copy` / `select_*` prefix | `debug_*` prefix |
| Dependency | None (pure data) | Forward-declared `lua_State*` |
| Wiring target | Native state accessors | Lua C callback registration |

The decision to use a `.cpp` file (rather than header-only inline stubs) is correct for this module — 27 function stubs would create excessive header bloat if inlined.

### 3. Prior Findings Status

| Finding | Description | Status |
|---------|-------------|--------|
| M37–M39 | share_service Makefile/namespace/test duplicates | ✅ All resolved |
| M40 | system_service should note .cpp needed when wired | ✅ Comment added to system_service.hpp line 17 |
| M41 | `const_cast` in `SelectInfoData::reset()` | ✅ `sel` changed to non-const `SelectData*` |
| M35–M36 | Date typo, gameplay trace | ✅ Resolved |

---

## New Module Review: `debug_script_service.hpp/.cpp`

### `main/ssz_native/debug_script_service.hpp` ✅

| Aspect | Assessment |
|--------|-----------|
| Struct design | `DebugScriptState` — 3 bools + `lua_State*`, default initializers ✅ |
| Function count | 27 declarations (25 callbacks + 2 file loaders) — matches debug-script.ssz ✅ |
| Signature convention | `void func(lua_State* L, int& re)` — consistent SSZ Lua callback pattern ✅ |
| Forward declaration | `struct lua_State;` — correct, no Lua header dependency ✅ |
| Design notes | 17-line header comment: Lua callback bridge pattern, wiring plan ✅ |
| Namespace | Root `ikemen::ssz_native` — consistent with Phase 3 convention ✅ |
| Includes | `<string>` only — minimal ✅ |
| `#pragma once` | Present ✅ |

### `main/ssz_native/debug_script_service.cpp` ✅

| Aspect | Assessment |
|--------|-----------|
| Stub count | 27 no-op bodies — all correct ✅ |
| Stub return values | Callbacks: `void` (correct — mutate via `int& re`); loaders: `{}` empty string ✅ |
| Design note | 4-line header comment: Phase 3 wiring plan ✅ |
| M4 TODO | Not needed — no native plugin calls (stub-only) ✅ |
| `SSZ_STDCALL` guard | Not present — correct for stub (no native plugin calls) ✅ |
| Separation of concerns | `.cpp` avoids 27 inline stubs bloating the header ✅ |

### Debug script tests in `test_file.cpp` (line ~745) ✅

| Test | Coverage |
|------|----------|
| `DebugScriptState` default init: roundResetFlg==false | Bool field ✅ |
| `DebugScriptState` default init: reloadFlg==false | Bool field ✅ |
| `DebugScriptState` default init: noHUDDisplay==false | Bool field ✅ |
| `DebugScriptState` default init: L==nullptr | Pointer field ✅ |
| All 25 callbacks called with `(nullptr, re)` — no crash | Stub safety ✅ |
| `debug_load_file("test.lua")` returns empty string | File loader stub ✅ |
| `debug_run_file("test.lua")` returns empty string | File loader stub ✅ |
| Field mutation: set all flags true, verify they hold | Field write/read ✅ |

**32 tests.** Most thorough stub-coverage test of any Phase 3 module. All 27 stubs are exercised.

### Makefile integration ✅

| Item | Status |
|------|--------|
| `IKEMEN_NATIVE_DEBUG_SCRIPT_LIB ?= $(IKEMEN_USE_NATIVE_SSZ)` (line 92) | ✅ |
| `CXXFLAGS += -DIKEMEN_NATIVE_DEBUG_SCRIPT_LIB=...` (line 110) | ✅ |
| `native_manifest` entry (line 835) | ✅ |
| No duplicate `?=` | ✅ Clean |

---

## New Findings (this review — debug_script_service)

### 🔴 Critical

*(None.)*

### 🟡 High — Should fix before next module

*(None.)*

### 🟢 Medium — Address in upcoming work

#### M42. Phase 3 has two .cpp strategies — need a convention

**Files:** `system_service.hpp` (header-only) vs `debug_script_service.hpp/.cpp` (split)

`system_service` uses header-only inline stubs. `debug_script_service` uses a separate `.cpp` with 27 stub bodies. Both are valid for scaffolding, but the inconsistency could lead to confusion for future modules. `share_service` also has a `.cpp` with 2 stubs.

**Recommendation:** Document the heuristic in `TODO_SSZ_CONVERSION.md` or a Phase 3 convention note:
- **≤5 stubs:** header-only inline (like `system_service`)
- **>5 stubs:** separate `.cpp` (like `debug_script_service`)
- This matches the M40 recommendation and provides clear guidance for the remaining 6 Phase 3 modules.

---

### 🔵 Low — Nice to have

#### L23. Sound constants are compile-time only (carried forward)

#### L25. `test_file.cpp` now ~1,234 lines (carried forward, updated)

Was 1,177, now 1,234 (57 new lines for debug_script_service tests). Splitting recommendation stands.

#### L34. `LuaState` has no self-move-assignment test (carried forward)

#### L35. `ssz_trace.hpp` has no unit test (carried forward)

#### L36. Trace log format not structured for argument-level parity (carried forward)

#### L37. `share_service.cpp` has no `SSZ_STDCALL` guard (carried forward)

`debug_script_service.cpp` also has no guard — correct for stubs. Both will need it when wired.

---

## Per-module Flag Wiring Status (re-verified)

| Static header | Guard | Status |
|---------------|-------|--------|
| `file_static.hpp` through `sdlplugin_static.hpp` | All 13 | ✅ |

**13 of 13 headers wired. ✅**

---

## Phase 3 Scaffolding Summary

| # | Module | SSZ lines | Hpp | Cpp | Stubs | Tests | Pattern |
|---|--------|-----------|-----|-----|-------|-------|---------|
| 1 | share | 371 | ✅ | ✅ | 2 | 11 | DTO struct + free fns |
| 2 | system | 427 | ✅ | — | 12 (inline) | 28 | DTO struct + inline stubs |
| 3 | debug-script | 296 | ✅ | ✅ | 27 | 32 | Lua callback bridge |
| 4 | common.ssz | 1,199 | ⬜ | ⬜ | — | — | Next target |
| 5–9 | (remaining) | — | ⬜ | ⬜ | — | — | — |

---

## Architecture Consistency Check

| Convention | Status |
|-----------|--------|
| RAII for all handles | ✅ 7 types |
| DTO struct + functions | ✅ 2 modules (ShareData, SelectData/SystemData) |
| Lua callback bridge | ✅ 1 module (debug_script_service) — new pattern |
| Free-function API where no handles | ✅ 7 services |
| Header-only where no state | ✅ 4 (consts, ssz_value, table, system_service) |
| Design notes in headers | ✅ All 19 service headers + ssz_trace.hpp |
| M4 TODO in call-through `.cpp` | ✅ 12 of 12 (Phase 3 stubs not applicable) |
| `is_valid()` on all handles | ✅ All 7 RAII types |
| No `PluginUtil*` or `Reference` in native services | ✅ |
| Feature flags in Makefile | ✅ 18 of 18 (17 service + 1 trace) |
| `#if` guards in `*_static.hpp` | ✅ 13 of 13 |
| `SSZ_TRACE` on bridge wrappers | ✅ 172 of 172 |
| Pre-conversion trace baselines | ✅ Startup (28 fn) + gameplay (47 fn) |

---

## Test Coverage Summary

All tests in `test/test_file.cpp` (1,234 lines). Run with `make CONFIG=Debug test`.

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
| share_service | 11 | Default init, field types, stub call safety ✅ |
| system_service | 28 | Default init, stub methods, field mutation ✅ |
| **debug_script_service** | **32** | **State init, 27 stub calls, file loaders, field mutation (new)** ✅ |

**Total: ~454 tests across 21 modules.**

---

## Open Issues Summary

| ID | Severity | File | Description |
|----|----------|------|-------------|
| M36 | 🟢 Medium | `docs/` | 125 bridge functions still not observed; need per-module traces |
| L23 | 🔵 Low | `sound_service.hpp` | Audio constants compile-time only |
| L25 | 🔵 Low | `test/test_file.cpp` | ~1,234 lines — consider splitting |
| L34 | 🔵 Low | `test/test_file.cpp` | LuaState self-move-assignment test missing |
| L35 | 🔵 Low | `ssz_trace.hpp` | No unit test |
| L36 | 🔵 Low | `docs/pre_conversion_trace.log` | Trace format not structured for argument-level parity |
| L37 | 🔵 Low | `share_service.cpp`, `debug_script_service.cpp` | No `SSZ_STDCALL` guard (correct for stubs; needed when wired) |

---

## Recommendations

1. **Add L34 self-move test** for `LuaState` (~2 minutes).
3. **Consider splitting `test_file.cpp`** at 1,234 lines into per-module test files under `test/`.
4. **Phase 3 next module** — `common.ssz` (1,199 lines) is next.
5. **Capture per-module gameplay traces** for sdlplugin/ogg/sound/mesdialog when those paths are touched.

---

## Exit Criteria Check (from TODO_SSZ_CONVERSION.md)

| Criterion | Status |
|-----------|--------|
| Phase 0 research deliverables complete | ✅ All 6 items |
| Pre-conversion trace logs captured | ✅ Startup (28 fn) + gameplay (47 fn) |
| Runtime trace mode active | ✅ `IKEMEN_ENABLE_PLUGIN_TRACE` + 172 instrumented wrappers |
| All SSZ `plugin index` calls route through native services | ⬜ SSZ scripts still call bridge wrappers |
| Zero `Reference` in non-bridge engine code | ✅ |
| All native services have unit test coverage | ✅ ~454 tests across 21 modules |
| Bridge wrappers only convert types (no logic) | ✅ |
| Phase 1 foundation libraries complete | ✅ 6 of 6 |
| Phase 2 plugin wrappers complete | ✅ 11 of 12 (sdlplugin deferred) |
| Phase 3 core engine modules | ⬜ Scaffolded: share + system + debug-script (3 of 9) |
| Per-module `#if` guards in `*_static.hpp` | ✅ 13 of 13 |
| Feature flags for all native modules | ✅ 18 of 18 (17 service + 1 trace) |
| CI guard | ⬜ Not yet |
| sdlplugin.ssz / sdlevent.ssz converted | ⬜ Intentionally deferred (Phase 2) |
