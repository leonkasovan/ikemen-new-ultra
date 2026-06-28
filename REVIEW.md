# REVIEW — Native C++ ABI Migration & SSZ Script Layer Conversion

**Review date:** 2026-06-28
**Scope:** All changes documented in `CHANGES.md`, including all post-review fix rounds (April 2026–February 2027), the new `alert_service` module, per-module flag wiring in 7 `*_static.hpp` headers, build system feature-flag infrastructure, and the complete `main/ssz_native/` module set (12 services, 23 files).

---

## Overall Assessment

The migration is in excellent shape. Twelve native service modules are implemented, all 53 previous review findings are resolved and source-verified, and the per-module feature-flag infrastructure is fully wired for all 7 previously-converted services. The new `alert_service` module follows the call-through pattern correctly but has minor omissions (see below).

**Code quality is high.** Every service follows established conventions: RAII handles where applicable, design notes, call-through vs bypass rationale, M4 TODO markers, and test coverage. The project has reached the point where new service modules can be added in minutes by following the established template.

**Verdict:** ✅ Excellent. Phase 2 plugin wrappers are nearly complete (lua and sdlplugin remain). The alert_service is a good example of how quickly simple services can now be added.

---

## Previous Review Findings — Complete Status (all rounds, source-verified)

| # | Finding | Round | Fix verified in source |
|---|---------|-------|------------------------|
| H1–H3, M1, M3, L3 | File/math plugin issues | Apr | ✅ All 6 fixes verified |
| H4, M4–M6, L5 | Include order, TODOs, AGENTS.md, self-move | Jun | ✅ All 5 fixes verified |
| H5–H6, M7–M9, L8 | math_service issues | Jul | ✅ All 6 fixes verified |
| H7–H8, M10–M11, L11 | regex_service issues | Aug | ✅ All 5 fixes verified |
| H9–H10, M12–M13 | socket_service issues | Sep | ✅ All 4 fixes verified |
| H11, M14–M16 | sound_service issues | Oct | ✅ All 4 fixes verified |
| H12–H13, M17–M19, L20, L22 | AGENTS.md, ranges, platform, docs | Nov | ✅ All 7 fixes verified |
| H14–H15, M21–M22 | AGENTS.md, ogg range, self-move, labels | Dec | ✅ All 4 fixes verified |
| H16–H17, M23, L27, M25p | AGENTS.md, mesdialog range, CodePage, file_static.hpp | Jan | ✅ All 5 fixes verified |
| H18–H19, M25–M27 | TEST_FILE_OBJS, AGENTS.md, flag wiring, CRYPTO_LIB, convention | Feb | ✅ All 5 fixes verified |

**All 53 findings resolved. ✅**

---

## New Module Review: `alert_service.hpp/.cpp`

### `main/ssz_native/alert_service.hpp` ✅

| Aspect | Assessment |
|--------|-----------|
| API | Single free function: `alert(title, message)` ✅ |
| Design note | 4-line call-through note ✅ |
| Includes | Only `<string>` — minimal ✅ |

### `main/ssz_native/alert_service.cpp` ⚠ (see M28, M29)

| Aspect | Assessment |
|--------|-----------|
| Native declaration | `void SSZ_STDCALL Alert(...)` — correct ✅ |
| Implementation | 1-line wrapper: `Alert(title, message)` ✅ |
| `SSZ_STDCALL` guard | Local `#ifndef` — necessary since header doesn't include `sszdef.h` ✅ |
| M4 TODO comment | ❌ Missing — no "duplicates bridge.cpp:…" note (see M29) |
| Design note | ✅ Present in header |

### Makefile integration ✅

| Item | Status |
|------|--------|
| `$(SSZ_NATIVE)/alert_service.cpp` in `MAIN_SRCS` (line 173) | ✅ |
| `$(BLD)/main/ssz_native/alert_service.o` in `TEST_FILE_OBJS` (line 768) | ✅ |
| `$(BLD)/main/alert/alert.o` in `TEST_FILE_OBJS` (native plugin) | ✅ |
| `IKEMEN_NATIVE_ALERT_LIB` flag (line 84) with `-D` define (line 94) | ✅ |
| `native_manifest` entry (line 795) | ✅ |

---

## New Findings (this review — June 2026, alert_service added)

### 🔴 Critical

*(None.)*

### 🟡 High — Should fix before next module

#### H20. `AGENTS.md` ssz_native row says "21 files" — now 23 files

**File:** `AGENTS.md`, line 15

```markdown
| SSZ native layer | `main/ssz_native/*` | 21 files, ~3,500 total |
```

`alert_service.hpp/.cpp` brings the total to **23 files** (~3,700 lines).

**Recommendation:** Update to `23 files, ~3,700 total`.

---

### 🟢 Medium — Address in upcoming work

#### M28. `alert_static.hpp` not wired with `#if IKEMEN_NATIVE_ALERT_LIB` guard

**File:** `main/alert_static.hpp`

The `IKEMEN_NATIVE_ALERT_LIB` flag exists in the Makefile (line 84) and is passed as a `-D` define, but `alert_static.hpp` has no `#if IKEMEN_NATIVE_ALERT_LIB` / `#else` stub guard. All 7 other static headers with corresponding `ssz_native` services have this guard. `alert_static.hpp` should be wired to match.

**Recommendation:** Apply the standard `#if IKEMEN_NATIVE_ALERT_LIB` / `#else` stub pattern to `alert_static.hpp`.

---

#### M29. `alert_service.cpp` missing M4 TODO comment

**File:** `main/ssz_native/alert_service.cpp`

Every other call-through service `.cpp` (socket, sound, ogg, mesdialog) documents which bridge.cpp lines its native declarations duplicate, with a TODO referencing M4 consolidation in `plugin_native_api.hpp`. `alert_service.cpp` has a single native declaration without this comment.

**Recommendation:** Add the standard M4 TODO comment above the `Alert` declaration:
```cpp
// Native alert plugin function (defined in main/alert/alert.cpp).
// This declaration duplicates bridge.cpp:<line>. Tracked in
// plugin_native_api.hpp's M4 TODO for eventual consolidation.
```

---

### 🔵 Low — Nice to have

#### L23. Sound constants are compile-time only (carried forward)

#### L25. `test_file.cpp` now ~920 lines covering 11 modules (carried forward)

`alert_service` has no tests in `test_file.cpp` (see L32). The file size is unchanged from the previous review.

---

#### L32. No `alert_service` smoke test

**File:** `test/test_file.cpp`

Unlike all other services (including stateless `mesdialog_service` with 4 tests), `alert_service` has no test function. A minimal no-crash smoke test — `alert(L"test", L"test")` — would be consistent with the sound/socket/ogg no-crash pattern, even though the dialog can't be verified programmatically.

**Recommendation:** Add a `test_alert_service()` with a single no-crash test: `ikemen::ssz_native::alert::alert(L"test", L"test smoke")`.

---

## Per-module Flag Wiring Status

| Static header | Guard | Status |
|---------------|-------|--------|
| `file_static.hpp` | `#if IKEMEN_NATIVE_FILE_LIB` | ✅ |
| `math_static.hpp` | `#if IKEMEN_NATIVE_MATH_LIB` | ✅ |
| `regex_static.hpp` | `#if IKEMEN_NATIVE_REGEX_LIB` | ✅ |
| `socket_static.hpp` | `#if IKEMEN_NATIVE_SOCKET_LIB` | ✅ |
| `sound_static.hpp` | `#if IKEMEN_NATIVE_SOUND_LIB` | ✅ |
| `ogg_static.hpp` | `#if IKEMEN_NATIVE_OGG_LIB` | ✅ |
| `mesdialog_static.hpp` | `#if IKEMEN_NATIVE_MESDIALOG_LIB` | ✅ |
| `alert_static.hpp` | — | ⬜ Flag exists, guard not wired (M28) |
| `lua_static.hpp` | — | ⬜ No lua_service yet |
| `sdlplugin_static.hpp` | — | ⬜ No sdl_service yet |
| `ssz_static.hpp` | — | ⬜ Infrastructure (runtime) |
| `thread_static.hpp` | — | ⬜ No thread_service yet |
| `time_static.hpp` | — | ⬜ No time_service yet |
| `shell_static.hpp` | — | ⬜ No shell_service yet |

---

## File-by-File Review (fresh assessment)

### Phase 1 — Foundation libraries (no plugin dependency)

| Service | Type | Assessment |
|---------|------|-----------|
| `ssz_value.hpp` | Types | ✅ |
| `consts.hpp` | Constants | ✅ |
| `math_service` | Header+impl | ✅ 15 inline wrappers, Park-Miller PRNG, 6 utilities |
| `string_service` | Header+impl | ✅ 10+ functions, UTF-8, percent, hex/octal |
| `crypto_service` | Header+impl | ✅ Base64, Arcfour RC4 |

### Phase 2 — Plugin wrapper libraries (call-through to native plugin)

| Service | Type | Assessment |
|---------|------|-----------|
| `file_service` | RAII + free fns | ✅ `FileHandle`, 12 free functions |
| `regex_service` | RAII | ✅ `Regex` with `std::wregex`/`boost::wregex` |
| `socket_service` | RAII | ✅ `SocketHandle` with `is_open()` |
| `sound_service` | RAII | ✅ `AudioClient` with `is_valid()` |
| `ogg_service` | RAII | ✅ `OggVorbisHandle` with `is_valid()` |
| `mesdialog_service` | Free fns | ✅ `CodePage` enum, 12 wrapped functions |
| `alert_service` | Free fn | ✅ Single `alert()` wrapper (new — see M28, M29, L32) |

### Shared infrastructure

| File | Assessment |
|------|-----------|
| `plugin_native_api.hpp` | ✅ Single source of truth, M4 TODO present |

---

## Test Coverage Summary

All tests in `test/test_file.cpp`. Run with `make CONFIG=Debug test`.

| Module | Test function | Approx. tests | Coverage |
|--------|---------------|---------------|----------|
| File plugin (native) | 10 functions | ~50 | All file operations ✅ |
| Math plugin (native) | `test_math` | 17 | 15 trig/exp functions ✅ |
| Thread plugin (native) | `test_thread` | 1 | ThreadDelay smoke ✅ |
| math_service | `test_math_service` | ~85 | Full coverage ✅ |
| string_service | `test_string_service` | ~23 | Full coverage ✅ |
| crypto_service | `test_crypto_service` | 10 | Base64 + Arcfour ✅ |
| mesdialog_service | `test_mesdialog_service` | 4 | Shared string, codepage ✅ |
| ogg_service | `test_ogg_service` | 8 | Full RAII coverage ✅ |
| sound_service | `test_sound_service` | 5 | Construction, move, start/stop ✅ |
| socket_service | `test_socket_service` | 7 | Construction, move, double-close ✅ |
| regex_service | `test_regex_service` | ~18 | Full coverage ✅ |
| file_service (RAII) | `test_file_handle` + edges | ~23 | Full coverage ✅ |
| alert_service | *(none)* | 0 | ⬜ Missing (L32) |

**Total: ~350 tests across 11 of 12 modules.** (alert_service untested)

---

## Build Integration ✅

| Component | Status |
|-----------|--------|
| `$(SSZ_NATIVE)` variable | ✅ |
| All 10 `.cpp` in `MAIN_SRCS` | ✅ |
| Compile rule | ✅ |
| 10 feature flags via `CXXFLAGS` | ✅ (FILE, STRING, MATH, REGEX, SOCKET, SOUND, OGG, MESDIALOG, CRYPTO, ALERT) |
| `#if` convention documented | ✅ `Makefile:70-74` |
| `TEST_FILE_OBJS` complete | ✅ |
| `make native_manifest` | ✅ 10 flags |
| 7 `*_static.hpp` guards | ✅ (alert_static.hpp pending — M28) |

---

## Architecture Consistency Check

| Convention | Status |
|-----------|--------|
| RAII for all handles | ✅ 6 RAII types |
| Free-function API where no handles | ✅ mesdialog, crypto (base64), alert |
| Design notes in headers | ✅ All 10 service headers |
| M4 TODO in call-through `.cpp` files | ⚠ 5 of 6 (alert_service missing — M29) |
| `is_valid()` / `is_open()` on all handles | ✅ All 6 RAII types |
| No `PluginUtil*` or `Reference` in native services | ✅ |
| Test coverage for all modules | ⚠ 11 of 12 (alert_service missing — L32) |
| Per-module feature flags in Makefile | ✅ 10 of 10 |
| Per-module `#if` guards in `*_static.hpp` | ⚠ 7 of 8 applicable (alert_static.hpp pending — M28) |

---

## Open Issues Summary

| ID | Severity | File | Description |
|----|----------|------|-------------|
| H20 | 🟡 High | `AGENTS.md:15` | "21 files, ~3,500" → should be "23 files, ~3,700" |
| M28 | 🟢 Medium | `alert_static.hpp` | Missing `#if IKEMEN_NATIVE_ALERT_LIB` guard (flag exists, header not wired) |
| M29 | 🟢 Medium | `alert_service.cpp` | Missing M4 TODO comment with bridge.cpp line reference |
| L23 | 🔵 Low | `sound_service.hpp` | Audio constants compile-time only |
| L25 | 🔵 Low | `test/test_file.cpp` | ~920 lines, consider splitting |
| L32 | 🔵 Low | `test/test_file.cpp` | No alert_service smoke test |

---

## Recommendations for Next Module

1. **Fix H20, M28, M29** (trivial, ~5 minutes total).
2. **Add L32 alert smoke test** (~2 minutes).
3. **Next Phase 2 target: lua service** — the last significant Phase 2 module (~20 functions). Wire `lua_static.hpp` guard as part of implementation.
4. **Consider `thread_service` and `time_service`** — each is a single function. Together with alert_service, they'd complete the Phase 2 simple-plugin wrappers.
5. **sdlplugin service last** — 60+ functions, complex SDL object lifetimes.

---

## Exit Criteria Check (from TODO_SSZ_CONVERSION.md)

| Criterion | Status |
|-----------|--------|
| All SSZ `plugin index` calls route through native services | ⬜ Not yet |
| Zero `Reference` in non-bridge engine code | ✅ |
| All native services have unit test coverage | ⚠ 11 of 12 (alert_service missing — L32) |
| Bridge wrappers only convert types (no logic) | ✅ |
| Per-module `#if` guards in `*_static.hpp` | ⚠ 7 of 8 applicable (alert_static pending — M28) |
| Feature flags for all native modules in Makefile | ✅ 10 of 10 |
| CI guard | ⬜ Not yet |
