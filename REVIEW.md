# REVIEW — Native C++ ABI Migration & SSZ Script Layer Conversion

**Review date:** 2026-06-28
**Scope:** All changes documented in `CHANGES.md`, including five rounds of post-review fixes and the new `sound_service.hpp/.cpp`.

---

## Overall Assessment

The migration work maintains good quality across five review cycles. The new `sound_service.hpp/.cpp` implements Phase 2 of `TODO_SSZ_CONVERSION.md` and follows the call-through pattern (delegating to native plugin functions). All 26 previous findings are resolved. The project is tracking well.

**Verdict:** ✅ Good to proceed to the next module.

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
| H4 | Include order | Jun | ✅ Swapped + comment |
| M4 | Missing TODO for remaining plugins | Jun | ✅ TODO + phase refs |
| M5 | `consts.hpp` type alias naming | Jun | ✅ Fixed-width comment |
| M6 | `ssz_native/` missing from `AGENTS.md` | Jun | ✅ Row added |
| L5 | Self-move-assignment test | Jun | ✅ Test + pragma |
| H5 | `math_service.hpp` misleading comment | Jul | ✅ Design note |
| H6 | `randI` signed overflow UB | Jul | ✅ Cast before subtraction |
| M7 | `round` grouped under "wrappers" | Jul | ✅ Own section |
| M8 | `randF` float precision | Jul | ✅ `double` intermediates |
| M9 | Missing design comment in `math_service` | Jul | ✅ Design note added |
| L8 | No PRNG parity test | Jul | ✅ Reference test |
| H7 | `regex_service.hpp` missing design note | Aug | ✅ Design note added |
| H8 | `regex_service.cpp` redundant code | Aug | ✅ Cleaned + comment |
| M10 | Linux error message hardcoded | Aug | ✅ Explanation comment |
| M11 | Duplicate `Match`/`RegexMatchInfo` | Aug | ✅ Consolidate TODO |
| L11 | Extensions undocumented | Aug | ✅ Extension comments |
| H9 | `socket_service.hpp` unnecessary include | Sep | ✅ Removed |
| H10 | `socket_service.cpp` duplicated declarations | Sep | ✅ TODO referencing M4 |
| M12 | Redundant `friend` declarations | Sep | ✅ Removed both |
| M13 | Missing design note in `socket_service` | Sep | ✅ Call-through rationale added |

All 26 previous findings are resolved. ✅

---

## September Review Fixes — Verification

| Fix | File | Verified |
|-----|------|----------|
| H9: Remove `ssz_value.hpp` include | `socket_service.hpp` | ✅ Include removed; only `<cstdint>` and `<string>` remain |
| H10: TODO comment for M4 consolidation | `socket_service.cpp` | ✅ "These declarations duplicate bridge.cpp:70-80. They are tracked in plugin_native_api.hpp's M4 TODO for eventual consolidation." |
| M12: Remove redundant `friend` | `socket_service.hpp` | ✅ Both `friend class SocketHandle` and `friend SocketHandle::accept()` removed — grep confirms zero `friend` occurrences |
| M13: Design note | `socket_service.hpp` | ✅ 6-line note explaining call-through rationale (OS handle types, Winsock/BSD init, mirrors file_service) |

---

## New Findings (this round)

### 🔴 Critical

*(None.)*

### 🟡 High — Should fix before next module

#### H11. `sound_service.cpp` duplicates native declarations without TODO comment

**File:** `main/ssz_native/sound_service.cpp`, lines 9–15

```cpp
// Native sound plugin functions (defined in main/sound/sound.cpp)
Client*   SSZ_STDCALL NewClient();
void      SSZ_STDCALL DeleteClient(Client* client);
bool      SSZ_STDCALL ClientStart(Client* client);
bool      SSZ_STDCALL ClientStop(Client* client);
bool      SSZ_STDCALL ClientBufferReady(Client* client);
bool      SSZ_STDCALL ClientSetBuffer(const float* buffer, intptr_t frames, Client* client);
```

These 6 declarations duplicate `bridge.cpp` (lines ~85–90). This is the same pattern as H10 for `socket_service.cpp`, which was fixed with a TODO comment referencing M4 consolidation. `sound_service.cpp` has no such comment.

**Recommendation:** Add the same style of TODO comment as `socket_service.cpp`:
```cpp
// Native sound plugin functions (defined in main/sound/sound.cpp).
// These declarations duplicate bridge.cpp:85-90. They are tracked in
// plugin_native_api.hpp's M4 TODO for eventual consolidation.
// TODO: Move to plugin_native_api.hpp when sound is migrated (Phase 2).
```

### 🟢 Medium — Address in upcoming work

#### M14. `AudioClient` has no `is_open()`/`is_valid()` method

**File:** `main/ssz_native/sound_service.hpp`

`FileHandle` and `SocketHandle` both expose `is_open()` to check whether the underlying resource is valid. `AudioClient` stores an opaque `void* client_` that can be null (if `NewClient()` fails or after move), but there is no public way to check it. All methods guard internally against null and return `false`, so there's no crash risk — but callers can't distinguish "operation failed" from "client was never created."

**Recommendation:** Add `bool is_valid() const { return client_ != nullptr; }` for API consistency with the other RAII handles.

#### M15. No design note in `sound_service.hpp`

**File:** `main/ssz_native/sound_service.hpp`

The design-note convention is now established: `math_service.hpp` (bypass rationale), `regex_service.hpp` (bypass rationale), `socket_service.hpp` (call-through rationale). `sound_service.hpp` has no such note, despite following the call-through pattern.

**Recommendation:** Add a 3–4 line design note explaining:
```cpp
// Design note: sound_service delegates to the SSZ native plugin layer
// (main/sound/sound.cpp). Audio operations depend on platform-specific
// audio backends managed by the native plugin (PortAudio/SDL_mixer).
// Using the call-through pattern (mirroring file_service and socket_service)
// avoids duplicating audio device initialization and buffer management.
```

#### M16. `AudioClient` constructor eagerly creates the client

**File:** `main/ssz_native/sound_service.hpp`, line 26 / `sound_service.cpp`, line 18

```cpp
AudioClient::AudioClient() {
    client_ = static_cast<void*>(NewClient());  // eagerly allocates
}
```

Unlike `FileHandle` and `SocketHandle` (which start closed and require an explicit `open()`/`connect()` call), `AudioClient` eagerly calls `NewClient()` in the constructor. If `NewClient()` returns `nullptr`, the object is silently non-functional. This matches SSZ's `let c = Client()` pattern, but the difference from other handles should be documented.

**Recommendation:** Add a brief comment in the header noting the eager-creation design choice.

### 🔵 Low — Nice to have

#### L17. Sound tests only verify no-crash (5 tests)

**File:** `test/test_file.cpp`, `test_sound_service()`

The 5 tests verify: default construction, move constructor, move assignment, and calling `start()`/`stop()`/`buffer_ready()` without crashing. No audio behavior can be verified in a unit test (audio requires hardware). This is expected — same limitation as socket tests (L14).

#### L18. Audio constants are hardcoded

**File:** `main/ssz_native/sound_service.hpp`, lines 12–14

```cpp
inline constexpr int FREQ = 48000;
inline constexpr int CHANNELS = 2;
inline constexpr int BUFFER_SAMPLES = 2048;
```

These values match the SSZ sound library defaults. If the SSZ library supports configuration at runtime (e.g., different sample rates), these constants would need to become variables. For now, this is fine as a Phase 2 implementation.

#### L19. `test_file.cpp` now ~740 lines covering 7 modules

The "consider splitting" recommendation (L12, L15) stands. The test file grows by ~6–10 lines per service module.

---

## New File Reviews

### `main/ssz_native/sound_service.hpp` ✅ (with M14, M15, M16 noted)

| Aspect | Assessment |
|--------|-----------|
| RAII class | Clean: move-only, non-copyable, destructor calls `destroy()` ✅ |
| Opaque handle | `void* client_` — appropriate for opaque `Client*` from native plugin ✅ |
| `start()`/`stop()` | Guards against null, delegates to native plugin ✅ |
| `buffer_ready()`/`set_buffer()` | Same guard + delegate pattern ✅ |
| Constants | `FREQ=48000`, `CHANNELS=2`, `BUFFER_SAMPLES=2048` — match SSZ defaults ✅ |
| `is_valid()` | Missing — inconsistent with FileHandle/SocketHandle (see M14) |
| Design note | Missing (see M15) |
| Eager creation | Different pattern from other handles, undocumented (see M16) |
| Includes | Only `<cstdint>` — minimal ✅ |

### `main/ssz_native/sound_service.cpp` ✅ (with H11 noted)

| Aspect | Assessment |
|--------|-----------|
| `AudioClient()` | Calls `NewClient()` eagerly. Null result silently absorbed ✅ |
| `destroy()` | Guards null, calls `DeleteClient`, nulls pointer. Called from destructor and move-assignment ✅ |
| `start()`/`stop()`/etc. | Guard null + delegate. Consistent return value (false on null) ✅ |
| `Client` forward declaration | `class Client;` — correct, matches `main/sound/sound.cpp` ✅ |
| Native declarations | 6 declarations duplicated from `bridge.cpp` without TODO (see H11) |
| `static_cast<Client*>` | `void*` ↔ `Client*` casts — sound/safe in this context ✅ |

### Sound service tests (`test_file.cpp:441–467`) ✅

| Test | Coverage |
|------|----------|
| Default construction | ✅ |
| Move constructor — no crash | ✅ |
| Move assignment — no crash | ✅ |
| `start()`/`stop()`/`buffer_ready()` — no crash | ✅ |

5 tests, RAII + basic API smoke. Audio operations not testable in unit tests (see L17).

### Makefile integration ✅

- `$(SSZ_NATIVE)/sound_service.cpp` added to `MAIN_SRCS` ✅
- `$(BLD)/main/ssz_native/sound_service.o` added to `TEST_FILE_OBJS` ✅
- `$(BLD)/main/sound/sound.o` added to `TEST_FILE_OBJS` (needed for linking) ✅

---

## Architecture Check (updated)

### `ssz_native/` module patterns — 6 modules, 2 approaches

| Module | Approach | Phase | Design note? |
|--------|----------|-------|--------------|
| `file_service` | Call-through | 1 | N/A (self-documenting) |
| `math_service` | Bypass | 1 | ✅ |
| `regex_service` | Bypass | 2 | ✅ |
| `socket_service` | Call-through | 2 | ✅ |
| `sound_service` | Call-through | 2 | ❌ Missing (M15) |
| `consts.hpp` | Pure data | 1 | N/A |
| `ssz_value.hpp` | Pure types | 1 | N/A |

The design-note convention is now well-established. `sound_service.hpp` is the only module missing one. Once added, the convention is complete.

### Key architectural differences between handles

| Handle | Creation | Validity check | Cleanup |
|--------|----------|----------------|---------|
| `FileHandle` | Explicit `open()` | `is_open()` | Destructor + `close()` |
| `SocketHandle` | Explicit `connect()`/`listen()` | `is_open()` | Destructor + `close()` |
| `AudioClient` | Eager in constructor | **Missing** (M14) | Destructor + `destroy()` (private) |
| `Regex` | Explicit `compile()` | `is_compiled()` | Destructor + `clear()` (private) |

The inconsistency in `AudioClient`'s API (eager creation, no validity check) should be documented even if the eager-creation pattern is intentional.

---

## Summary

| Category | Count | Status |
|---|---|---|
| Critical | 0 | — |
| High | 1 | H11: missing TODO comment on duplicated declarations |
| Medium | 3 | M14: missing `is_valid()`; M15: missing design note; M16: eager creation undocumented |
| Low | 3 | L17–L19: audio test gap, hardcoded constants, test file size |

**Bottom line:** Five rounds of review fixes applied correctly. All 26 previous findings resolved. The new `sound_service` is a clean RAII wrapper following the established call-through pattern. One high finding (H11, missing TODO comment — same as H10 pattern fixed for socket) and three medium findings (API consistency and documentation). The project continues to track well against `TODO_SSZ_CONVERSION.md`.
