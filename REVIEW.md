# REVIEW — Native C++ ABI Migration & SSZ Script Layer Conversion

**Review date:** 2026-07-01
**Scope:** Phase 3 underway (6 modules scaffolded), gameplay trace captured, full bridge trace instrumentation, pre-conversion trace baselines, all `main/ssz_native/` modules (44 files, 22 services), and resolution of all prior findings.

---

## Overall Assessment

The migration is in excellent shape. Phase 3 (core engine SSZ module conversion) now has six modules scaffolded — two-thirds through the 9-module Phase 3 target:

| # | Module | SSZ lines | Native files | Pattern | Stubs | Tests |
|---|--------|-----------|-------------|---------|-------|-------|
| 1 | `share.ssz` | 371 | `share_service.hpp/.cpp` | DTO struct + free fns | 2 | 11 |
| 2 | `system.ssz` | 427 | `system_service.hpp` (header-only) | DTO struct + inline stubs | 12 | 28 |
| 3 | `debug-script.ssz` | 296 | `debug_script_service.hpp/.cpp` | Lua callback bridge | 27 | 32 |
| 4 | `loader.ssz` | 284 | `loader_service.hpp/.cpp` | State machine + free fns | 7 | 13 |
| 5 | `common.ssz` | 1,199 | `common_service.hpp/.cpp` | DTO struct + free fns | 17 | ~30 |
| 6 | `trigger-script.ssz` | 1,633 | `trigger_script_service.hpp/.cpp` | Lua registration bridge | 1 | 2 |

The trigger_script_service introduces a **minimalist scaffolding pattern** for large callback-heavy modules. With 170+ Lua trigger functions, declaring individual stubs would create excessive boilerplate (~340 lines of declarations + ~170 lines of empty bodies). Instead, the module declares a single `register_function(lua_State*)` entry point with a 20-line header comment documenting all callback categories. This is a pragmatic divergence from `debug_script_service` (which declared all 25 callbacks individually — reasonable at that scale) and establishes a valid third approach for Phase 3.

**Code quality remains high.** The header comment is thorough — it lists 14 callback categories (player/parent/root/helper/target/partner/enemy/enemynear, alive/ctrl/statetype/movetype/physics/anim/state, life/power/attack/defence, pos/vel/facing, edge detection, round/match state, hitdef/hitcount, var/fvar/svar/tvar) and explains the wiring plan. The 2 tests cover state creation and stub no-crash.

**Verdict:** ✅ Excellent. No critical or high-severity findings. Phase 3 is 6 of 9 modules scaffolded — two-thirds complete.

---

## What's New Since Last Review (2026-07-01, common_service)

### 1. Phase 3 Continued — trigger_script_service Scaffolding ✅

The sixth Phase 3 module (`ssz_script/ssz/trigger-script.ssz`, 1,633 lines) has been scaffolded:

| Deliverable | Details |
|-------------|---------|
| `main/ssz_native/trigger_script_service.hpp` | **NEW** — `TriggerScriptState` struct + `register_function` declaration + callback category documentation |
| `main/ssz_native/trigger_script_service.cpp` | **NEW** — `register_function` stub (no-op) |
| `IKEMEN_NATIVE_TRIGGER_SCRIPT_LIB` flag | Added to Makefile with `?=` default and `-D` define |
| `test/test_file.cpp` | Added `test_trigger_script_service()` — 2 tests |

**API surface:**
- `TriggerScriptState` — minimal struct (opaque `cwc` character pointer commented for wiring)
- `register_function(lua_State*)` — single entry point stub; will register all 170+ callbacks when wired
- **170+ callbacks deferred** — header comment documents 14 categories: player/character queries, state type checks, life/power/attack/defence stats, position/velocity/facing, edge detection, round/match state, hitdef/hitcount, var/fvar/svar/tvar

### 2. New Scaffolding Pattern: Minimalist Registration Bridge

This module establishes a third Phase 3 pattern for callback-heavy modules:

| Aspect | Full declaration (debug_script, 27 stubs) | Minimalist (trigger_script, 170+ stubs) |
|--------|------------------------------------------|----------------------------------------|
| Stub count | 27 individual declarations + bodies | 1 `register_function` entry point |
| Header lines | ~95 (declarations + struct) | ~55 (struct + documentation) |
| `.cpp` lines | ~35 (27 empty bodies) | ~10 (1 empty body) |
| Boilerplate per callback | ~2 lines | 0 lines |
| When to use | ≤50 callbacks | >50 callbacks |
| Wiring strategy | Implement each stub individually | Implement all in `register_function` body |

This is a valid and pragmatic divergence. The 20-line header comment documenting all callback categories serves as the specification that individual declarations would otherwise provide.

### 3. Prior Findings Status

| Finding | Description | Status |
|---------|-------------|--------|
| M42–M43 | .cpp strategy, unused include | ✅ Resolved |
| M40–M41 | system_service issues | ✅ Resolved |
| M44–M46 | common_service observations | ⬜ Open (deferred to wiring) |
| M36 | Gameplay trace coverage | ⬜ Open |

---

## New Module Review: `trigger_script_service.hpp/.cpp`

### `main/ssz_native/trigger_script_service.hpp` ✅

| Aspect | Assessment |
|--------|-----------|
| Struct design | `TriggerScriptState` — minimal, opaque `cwc` reference commented ✅ |
| Stub strategy | Single `register_function` — pragmatic for 170+ callbacks ✅ |
| Callback documentation | 20-line comment listing 14 categories with specific function names ✅ |
| Forward declaration | `struct lua_State;` — correct ✅ |
| Design notes | 10-line header comment: wiring plan, callback pattern explanation ✅ |
| Namespace | Root `ikemen::ssz_native` — consistent ✅ |
| Includes | None needed (forward decl only) — exceptionally minimal ✅ |
| `#pragma once` | Present ✅ |

### `main/ssz_native/trigger_script_service.cpp` ✅

| Aspect | Assessment |
|--------|-----------|
| Stub body | `register_function` — empty with Phase 3 TODO comment ✅ |
| Design note | 3-line header comment ✅ |
| M4 TODO | Not needed — no native plugin calls ✅ |
| `SSZ_STDCALL` guard | Not present — correct for stub ✅ |

### Trigger script tests in `test_file.cpp` (line ~748) ✅

| Test | Coverage |
|------|----------|
| `TriggerScriptState` created | Struct compilation ✅ |
| `register_function(nullptr)` — no crash | Stub safety ✅ |

**2 tests.** Minimal but appropriate — the module has only one stub and an effectively empty struct. The tests verify compilation and no-crash for the single entry point.

### Makefile integration ✅

| Item | Status |
|------|--------|
| `IKEMEN_NATIVE_TRIGGER_SCRIPT_LIB ?= $(IKEMEN_USE_NATIVE_SSZ)` (line 95) | ✅ |
| `CXXFLAGS += -DIKEMEN_NATIVE_TRIGGER_SCRIPT_LIB=...` (line 116) | ✅ |
| `native_manifest` entry (line 850) | ✅ |
| No duplicate `?=` | ✅ Clean |

---

## New Findings (this review — trigger_script_service)

### 🔴 Critical

*(None.)*

### 🟡 High — Should fix before next module

*(None.)*

### 🟢 Medium — Address in upcoming work

#### M47. Callback stubs deferred — 170+ functions not individually declared

**File:** `main/ssz_native/trigger_script_service.hpp`

Unlike `debug_script_service` (which declares all 25 callback stubs individually), `trigger_script_service` defers all 170+ callbacks to a single `register_function` entry point. This is pragmatic — 170 individual declarations would create ~340 lines of boilerplate — but it means there's no per-function compile-time verification that the stub set matches the SSZ source.

**Recommendation:** When wiring, either:
- (A) Add individual declarations incrementally as callbacks are implemented, or
- (B) Keep the single `register_function` approach and add a script that verifies the callback list against `trigger-script.ssz`.
The header comment documenting all 14 categories serves as an adequate specification for now.

---

### 🔵 Low — Nice to have

#### L23. Sound constants are compile-time only (carried forward)

#### L25. `test_file.cpp` now ~1,360 lines (carried forward, updated)

Was 1,342, now 1,360 (18 new lines for trigger_script_service tests). Splitting recommendation stands.

#### L34. `LuaState` has no self-move-assignment test (carried forward)

#### L35. `ssz_trace.hpp` has no unit test (carried forward)

#### L36. Trace log format not structured for argument-level parity (carried forward)

#### L37. All Phase 3 stub `.cpp` files lack `SSZ_STDCALL` guard (carried forward)

Now 6 `.cpp` files. Correct for stubs; all will need it when wired.

---

## Phase 3 Scaffolding Summary

| # | Module | SSZ lines | Hpp | Cpp | Stubs | Tests | Pattern |
|---|--------|-----------|-----|-----|-------|-------|---------|
| 1 | share | 371 | ✅ | ✅ | 2 | 11 | DTO struct + free fns |
| 2 | system | 427 | ✅ | — | 12 (inline) | 28 | DTO struct + inline stubs |
| 3 | debug-script | 296 | ✅ | ✅ | 27 | 32 | Lua callback bridge (full) |
| 4 | loader | 284 | ✅ | ✅ | 7 | 13 | State machine + free fns |
| 5 | common | 1,199 | ✅ | ✅ | 17 | ~30 | DTO struct + free fns |
| 6 | trigger-script | 1,633 | ✅ | ✅ | 1 (register) | 2 | Lua registration bridge (minimal) |
| 7 | `script.ssz` | 2,216 | ⬜ | ⬜ | — | — | **Next target** |
| 8 | `system-script.ssz` | 2,403 | ⬜ | ⬜ | — | — | — |
| 9 | `statebuilder.ssz` | 9,334 | ⬜ | ⬜ | — | — | Last |

---

## Architecture Consistency Check

| Convention | Status |
|-----------|--------|
| RAII for all handles | ✅ 7 types |
| DTO struct + free fns | ✅ 3 modules (ShareData, SelectData/SystemData, CommonData) |
| Lua callback bridge (full) | ✅ 1 module (debug_script_service) |
| Lua registration bridge (minimal) | ✅ 1 module (trigger_script_service) — new pattern |
| State machine + free fns | ✅ 1 module (loader_service) |
| Free-function API where no handles | ✅ 7 services |
| Header-only where no state | ✅ 4 (consts, ssz_value, table, system_service) |
| Design notes in headers | ✅ All 22 service headers + ssz_trace.hpp |
| M4 TODO in call-through `.cpp` | ✅ 12 of 12 (Phase 3 stubs not applicable) |
| `is_valid()` on all handles | ✅ All 7 RAII types |
| No `PluginUtil*` or `Reference` in native services | ✅ |
| Feature flags in Makefile | ✅ 21 of 21 (20 service + 1 trace) |
| `#if` guards in `*_static.hpp` | ✅ 13 of 13 |
| `SSZ_TRACE` on bridge wrappers | ✅ 172 of 172 |
| Pre-conversion trace baselines | ✅ Startup (28 fn) + gameplay (47 fn) |

---

## Test Coverage Summary

All tests in `test/test_file.cpp` (1,360 lines). Run with `make CONFIG=Debug test`.

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
| debug_script_service | 32 | State init, 27 stub calls, file loaders ✅ |
| loader_service | 13 | State init, 7 stubs, enum values ✅ |
| common_service | ~30 | Field defaults, enum values, point types, stub calls, mutation ✅ |
| **trigger_script_service** | **2** | **State creation, register_function stub (new)** ✅ |

**Total: ~499 tests across 24 modules.**

---

## Open Issues Summary

| ID | Severity | File | Description |
|----|----------|------|-------------|
| M36 | 🟢 Medium | `docs/` | 125 bridge functions still not observed; need per-module traces |
| M44 | 🟢 Medium | `common_service.hpp` | `static constexpr` inside struct — unusual DTO pattern |
| M45 | 🟢 Medium | `common_service.hpp` | `void* load_callback` — replace with typed callback when wired |
| M46 | 🟢 Medium | `test/test_file.cpp` | `common_load_file` test passes mutable string ref — correct but noted |
| M47 | 🟢 Medium | `trigger_script_service.hpp` | 170+ callbacks deferred — per-function compile-time verification lost |
| L23 | 🔵 Low | `sound_service.hpp` | Audio constants compile-time only |
| L25 | 🔵 Low | `test/test_file.cpp` | ~1,360 lines — consider splitting |
| L34 | 🔵 Low | `test/test_file.cpp` | LuaState self-move-assignment test missing |
| L35 | 🔵 Low | `ssz_trace.hpp` | No unit test |
| L36 | 🔵 Low | `docs/pre_conversion_trace.log` | Trace format not structured for argument-level parity |
| L37 | 🔵 Low | 6 stub `.cpp` files | No `SSZ_STDCALL` guard (correct for stubs; needed when wired) |

---

## Recommendations

1. **Add L34 self-move test** for `LuaState` (~2 minutes).
2. **Consider splitting `test_file.cpp`** at 1,360 lines into per-module test files under `test/`.
3. **Note M47** — When wiring trigger_script_service, decide between incremental per-function declarations or keeping the single `register_function` approach with a verification script.
4. **Phase 3 next module** — `script.ssz` (2,216 lines) or `system-script.ssz` (2,403 lines). These are the two remaining "medium-large" modules before the 9,334-line `statebuilder.ssz`. Both introduce the script execution engine — consider whether the DTO + free-fn pattern scales or if a class-based approach would be more appropriate.
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
| All native services have unit test coverage | ✅ ~499 tests across 24 modules |
| Bridge wrappers only convert types (no logic) | ✅ |
| Phase 1 foundation libraries complete | ✅ 6 of 6 |
| Phase 2 plugin wrappers complete | ✅ 11 of 12 (sdlplugin deferred) |
| Phase 3 core engine modules | ⬜ Scaffolded: 6 of 9 (share, system, debug-script, loader, common, trigger-script) |
| Per-module `#if` guards in `*_static.hpp` | ✅ 13 of 13 |
| Feature flags for all native modules | ✅ 21 of 21 (20 service + 1 trace) |
| CI guard | ⬜ Not yet |
| sdlplugin.ssz / sdlevent.ssz converted | ⬜ Intentionally deferred (Phase 2) |
