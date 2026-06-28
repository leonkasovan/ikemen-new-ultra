# REVIEW — Native C++ ABI Migration & SSZ Script Layer Conversion

**Review date:** 2026-06-28
**Scope:** All changes documented in `CHANGES.md`, including three rounds of post-review fixes and the new `regex_service.hpp/.cpp`.

---

## Overall Assessment

The migration work maintains good quality. Three rounds of review findings have been addressed. The new `regex_service.hpp/.cpp` implements Phase 2 of `TODO_SSZ_CONVERSION.md` and is a clean RAII wrapper with thorough tests. The project is tracking well against the conversion plan.

**Verdict:** ✅ Good to proceed. Fix H7 before the next module.

---

## Previous Review Findings — Status (all rounds)

| # | Finding | Round | Status |
|---|---------|-------|--------|
| H1 | Duplicated forward declarations | Apr | ✅ `plugin_native_api.hpp` created |
| H2 | `_ftelli64` missing `<io.h>` | Apr | ✅ Guarded include added |
| H3 | `SendWriteBGM` discarded param | Apr | ✅ ABI comment added |
| M1 | `GetRendererInfo` stub | Apr | ✅ TODO comment added |
| M3 | MinGW `__stdcall` fragility | Apr | ✅ Note in `TODO.md` |
| L3 | FileHandle edge case tests | Apr | ✅ 12 tests added |
| H4 | Include order `sszdef.h` vs `plugin_native_api.hpp` | Jun | ✅ Swapped + comment |
| M4 | Missing TODO for remaining plugins | Jun | ✅ TODO + phase refs added |
| M5 | `consts.hpp` type alias naming | Jun | ✅ SSZ fixed-width comment |
| M6 | `ssz_native/` missing from `AGENTS.md` | Jun | ✅ Row added |
| L5 | Self-move-assignment test | Jun | ✅ Test + pragma added |
| H5 | `math_service.hpp` misleading comment | Jul | ✅ Rewrote comment + design note |
| H6 | `randI` signed overflow UB | Jul | ✅ `int64_t` cast before subtraction |
| M7 | `round` grouped under "wrappers" | Jul | ✅ Moved to own section |
| M8 | `randF` float precision loss | Jul | ✅ Changed to `double` intermediates |
| M9 | Missing design comment in `math_service` | Jul | ✅ Design-note block added at top |
| L8 | No PRNG parity test | Jul | ✅ Park-Miller seed=1 test (16807, 282475249) |

All 17 previous findings are resolved. ✅

---

## July Review Fixes — Verification

| Fix | File | Verified |
|-----|------|----------|
| H5: Comment + design note | `math_service.hpp` | ✅ "Math functions reimplemented natively via the C standard library" + 7-line design note at top explaining intentional SSZ plugin bypass |
| H6: `int64_t` cast before subtraction | `math_service.cpp` | ✅ `int64_t x64 = x; int64_t y64 = y;` then `x64 - y64` — both overflow branches fixed |
| M7: `round` own section | `math_service.hpp` | ✅ "Rounding (service-layer addition, no SSZ native plugin equivalent)" with 3-line comment |
| M8: `double` intermediates | `math_service.cpp` | ✅ `double r = static_cast<double>(random()) * ...` + precision-limitation comment |
| M9: Design note | `math_service.hpp` | ✅ Design note covers: intentional bypass, rationale (identical results), independence benefit, migration path if plugin changes |
| L8: PRNG parity | `test_file.cpp` | ✅ `srand(1)` → `random()` = 16807, 2nd = 282475249 (verified against minimal standard generator reference) |

---

## New Findings (this round)

### 🔴 Critical

*(None.)*

### 🟡 High — Should fix before next module

#### H7. `regex_service.hpp` has no design note explaining SSZ plugin bypass

**File:** `main/ssz_native/regex_service.hpp`

`regex_service.hpp` creates `std::wregex`/`boost::wregex` directly — it does not call `main/regex/regex.cpp::NewRegex` (the SSZ native plugin function). This is the same independence pattern as `math_service`, but unlike `math_service.hpp` (which has a 7-line design note), `regex_service.hpp` has no explanation.

A reader encountering this file won't know:
- Whether the bypass is intentional or an oversight
- What the relationship is to `main/regex/regex.cpp`
- Whether this should call through `plugin_native_api.hpp` instead

**Recommendation:** Add a design-note block (matching the style in `math_service.hpp`) explaining:
```cpp
// Design note: regex_service creates std::wregex/boost::wregex directly rather than
// calling the SSZ native plugin (main/regex/regex.cpp's NewRegex). The SSZ native
// plugin uses the same ECMAScript+optimize flags and identical error-handling
// patterns. This module compiles and tests without linking against regex.o,
// making it independent of the SSZ runtime.
// If main/regex/regex.cpp ever adds SSZ-specific behavior, the Regex::compile
// implementation must be updated to call through plugin_native_api.hpp instead.
```

#### H8. `regex_service.cpp` redundantly redefines `SSZ_REGEX_NS` and re-includes headers

**File:** `main/ssz_native/regex_service.cpp`, lines 3–9

```cpp
#include "regex_service.hpp"    // already defines SSZ_REGEX_NS and #includes <regex>/<boost/regex.hpp>

#ifdef _WIN32
#include <windows.h>
#include <regex>                // ← already included via regex_service.hpp
#define SSZ_REGEX_NS std        // ← already defined to the same value
#else
#include <boost/regex.hpp>      // ← already included via regex_service.hpp
#define SSZ_REGEX_NS boost      // ← already defined to the same value
#endif
```

The `#include` guards prevent actual double-inclusion, and the `#define` to the same value is harmless. But this is redundant code that suggests the author wasn't aware the header already provides these. It also creates a maintenance risk — if someone changes the definition in the header but forgets the `.cpp`, the `.cpp` will silently use the old definition.

**Recommendation:** Remove lines 3–9 (the entire `#ifdef` block). The `windows.h` include (needed for `MultiByteToWideChar`) can be kept separately:
```cpp
#include "regex_service.hpp"
#ifdef _WIN32
#include <windows.h>   // MultiByteToWideChar, CP_THREAD_ACP
#endif
```

### 🟢 Medium — Address in upcoming work

#### M10. `regex_service` error handling on Linux returns hardcoded string

**File:** `main/ssz_native/regex_service.cpp`, line 30

```cpp
#else
    return L"regex error";
#endif
```

The SSZ native plugin (`main/regex/regex.cpp`) uses `PluginUtil::wToGw(PluginUtil::aToW(e.what()))` to convert the error message. `regex_service` can't use `PluginUtil` (it's independent of the SSZ runtime), so a hardcoded fallback is acceptable — but it should be documented as a known limitation. The Windows path correctly uses `MultiByteToWideChar` for the conversion; Linux users will get less information.

**Recommendation:** Add a comment above the `#else` branch:
```cpp
#else
    // Known limitation: regex_service is independent of the SSZ runtime
    // and cannot use PluginUtil for charset conversion. The SSZ native
    // plugin (main/regex/regex.cpp) produces a localized error message via
    // PluginUtil::wToGw(PluginUtil::aToW(e.what())). Returning a generic
    // message here is a deliberate trade-off for runtime independence.
    return L"regex error";
#endif
```

#### M11. `regex_service.hpp` uses different `Match` struct than `bridge.hpp`'s `RegexMatchInfo`

**Files:** `regex_service.hpp` (line 25), `bridge.hpp` (line 25)

`regex_service.hpp` defines:
```cpp
struct Match { intptr_t pos = 0; intptr_t len = 0; };
```

`bridge.hpp` defines:
```cpp
struct RegexMatchInfo { intptr_t pos; intptr_t len; };
```

These are semantically identical but structurally separate. If the SSZ script layer is fully replaced, `RegexMatchInfo` (in the bridge) becomes dead code, and `Match` (in regex_service) becomes the canonical type. During the transition, having two types for the same concept is confusing.

**Recommendation:** Add a comment in `regex_service.hpp` noting the relationship to `RegexMatchInfo`, and add a TODO to consolidate once the bridge layer is retired.

### 🔵 Low — Nice to have

#### L11. `search_matches()` and `search_all()` are service-layer extensions

**File:** `main/ssz_native/regex_service.hpp`

The SSZ native plugin only provides `RegexSearch` (single match with capture groups). `regex_service` adds `search_matches` (raw pos/len for all matches) and `search_all` (full text of all matches). These are new functionality beyond what the SSZ plugin offers.

**Recommendation:** Add a brief comment noting these are extensions beyond the SSZ plugin API.

#### L12. `test/test_file.cpp` now covers 5 modules — consider splitting

The test file tests: file plugin, math plugin, thread plugin, file_service, math_service, regex_service. At ~650 lines and growing, it will become hard to navigate. Consider splitting into per-module test files before the next service module is added.

#### L13. `bridge.cpp` line count unchanged (~1400 lines, 4 rounds)

Still acceptable. The "split at 2000 lines" recommendation stands.

---

## New File Reviews

### `main/ssz_native/regex_service.hpp` ✅ (with H7 noted)

| Aspect | Assessment |
|--------|-----------|
| RAII class | Clean: move-only, non-copyable, auto-cleanup in destructor ✅ |
| `compile()` | Returns `std::wstring` error (empty on success). Matches SSZ `NewRegex` pattern ✅ |
| `search()` | Returns capture groups as substrings. Matches SSZ `RegexSearch` ✅ |
| `search_matches()` | Returns raw pos/len. Extension beyond SSZ plugin (see L11) ✅ |
| `search_all()` | Returns all non-overlapping matches. Extension beyond SSZ plugin (see L11) ✅ |
| `is_compiled()` | Simple accessor ✅ |
| Move semantics | `delete` copy, `noexcept` move ✅ |
| `SSZ_REGEX_NS` | Cross-platform macro with `#undef` cleanup at EOF ✅ |
| Design note | Missing (see H7) |
| Free `compile()` | Returns `pair<Regex, wstring>`. Convenience wrapper ✅ |

### `main/ssz_native/regex_service.cpp` ✅ (with H8, M10 noted)

| Aspect | Assessment |
|--------|-----------|
| `Regex::compile()` | ECMAScript + optimize flags, case-insensitive option. Matches SSZ native `NewRegex` ✅ |
| Error handling (Windows) | `MultiByteToWideChar(CP_THREAD_ACP, ...)` — correct, same as SSZ native ✅ |
| Error handling (Linux) | Returns hardcoded `L"regex error"` (see M10) |
| `Regex::search()` | Uses `regex_search` + `wcmatch`. Returns capture groups as `wstring` ✅ |
| `Regex::search_matches()` | Iterative `regex_search`, tracks absolute positions. Correct ✅ |
| `Regex::search_all()` | Iterative `regex_search`, returns full match text. Correct ✅ |
| Redundant code | `SSZ_REGEX_NS` redefinition + header re-include (see H8) |
| `#undef SSZ_REGEX_NS` | Clean macro hygiene at EOF ✅ |

### Regex service tests (`test_file.cpp:439–517`) ✅

| Test | Coverage |
|------|----------|
| Simple pattern compile + `is_compiled` | ✅ |
| `search_all` — multiple matches | ✅ |
| `search` — capture groups (3 groups) | ✅ |
| `search` — no match returns empty | ✅ |
| Case insensitive flag | ✅ |
| Invalid pattern — error string + `!is_compiled` | ✅ |
| `search_matches` — raw pos/len verification | ✅ |
| Free function `compile` | ✅ |
| Move semantics — source invalidated, dest valid | ✅ |

14 regex tests, all passing. Well-structured and comprehensive. ✅

---

## Architecture Check (updated)

### `ssz_native/` layer independence

| Module | Depends on SSZ plugin ABI? | Depends on SSZ VM headers? | Design note? |
|--------|---------------------------|---------------------------|--------------|
| `file_service` | Yes (via `plugin_native_api.hpp`) | No | N/A (calls through plugin) |
| `math_service` | No (uses `<cmath>` directly) | No | ✅ Yes |
| `regex_service` | No (uses `<regex>`/`<boost/regex.hpp>` directly) | No | ❌ Missing (H7) |
| `consts.hpp` | No | No | N/A (pure data) |
| `ssz_value.hpp` | No | No | N/A (pure types) |

Two modules now use the "bypass SSZ plugin" pattern (`math_service`, `regex_service`). One has a design note, one doesn't. Standardize on having the note.

### Bridge resolution chain — unchanged, still correct ✅

---

## Summary

| Category | Count | Status |
|---|---|---|
| Critical | 0 | — |
| High | 2 | H7: missing design note; H8: redundant code in .cpp |
| Medium | 2 | M10: Linux error message; M11: duplicate Match/MatchInfo types |
| Low | 3 | L11–L13: extension docs, test file size, bridge.cpp size |

**Bottom line:** Three rounds of review fixes applied correctly. The new `regex_service` is clean and well-tested (14 tests covering compile, search, capture groups, error handling, move semantics). The two high findings (H7 missing design note, H8 redundant code) are quick documentation/cleanup fixes. The project is on track for the next `TODO_SSZ_CONVERSION.md` module.
