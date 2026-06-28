# SSZ Script To Native C++ Conversion Plan

## Goal

Convert the SSZ script tree under `ssz_script/` into maintainable native C++ while keeping the engine functional during migration.

This is separate from the plugin ABI migration in `main/*.cpp`:

- `main/ssz/bridge.cpp` handles **old SSZ plugin ABI → native plugin implementation**.
- This plan handles **SSZ script behavior → native C++ engine behavior**.

## Status (Updated)

**Pre-work completed:**
- Plugin ABI migration (Phases 1–4 of `TODO.md`) is done.
- All 14 static plugin headers declare bridge wrappers; `(void*)FunctionName` resolves to bridge symbols in `bridge.cpp` / `ssz.cpp`.
- The `Run(std::wstring&)` entry point in `main.cpp` is now native.
- 85 regression smoke tests pass for native plugins (file, math, thread, FileHandle).

**This plan therefore picks up where that work left off:** converting the *script layer* that calls those native plugins.

## Current Inventory

Automatic inventory from the repository:

- SSZ script files: **45**
- Total SSZ lines: **40211**
- `plugin index` declarations: **20**
- `lib ... = <...>` dependency declarations: **151**
- Approximate class/object declarations: **105**
- Approximate function-like declarations: **2314**

### Largest SSZ Files

- `ssz_script/ssz/statebuilder.ssz` — 9334 lines
- `ssz_script/ssz/char.ssz` — 7665 lines
- `ssz_script/ssz/fight.ssz` — 3578 lines
- `ssz_script/ssz/system-script.ssz` — 2403 lines
- `ssz_script/ssz/script.ssz` — 2216 lines
- `ssz_script/ssz/trigger-script.ssz` — 1633 lines
- `ssz_script/ssz/command.ssz` — 1572 lines
- `ssz_script/ssz/sff.ssz` — 1413 lines
- `ssz_script/ssz/common.ssz` — 1199 lines
- `ssz_script/lib/alpha/sdlplugin.ssz` — 1022 lines
- `ssz_script/lib/string.ssz` — 775 lines
- `ssz_script/ssz/stage.ssz` — 736 lines
- `ssz_script/ssz/bg.ssz` — 726 lines
- `ssz_script/ssz/fighting.ssz` — 671 lines
- `ssz_script/lib/alpha/sdlevent.ssz` — 599 lines
- `ssz_script/ssz/system.ssz` — 427 lines
- `ssz_script/ssz/font.ssz` — 409 lines
- `ssz_script/ssz/sound.ssz` — 408 lines
- `ssz_script/ssz/share.ssz` — 371 lines
- `ssz_script/ssz/debug-script.ssz` — 296 lines

### Plugin Boundary Files

- `ssz_script/lib/alpha/sdlplugin.ssz` — 6 plugin index declarations
- `ssz_script/lib/file.ssz` — 4 plugin index declarations
- `ssz_script/lib/socket.ssz` — 3 plugin index declarations
- `ssz_script/lib/alpha/ogg.ssz` — 2 plugin index declarations
- `ssz_script/lib/alpha/lua.ssz` — 1 plugin index declarations
- `ssz_script/lib/alpha/mesdialog.ssz` — 1 plugin index declarations
- `ssz_script/lib/regex.ssz` — 1 plugin index declarations
- `ssz_script/lib/sound.ssz` — 1 plugin index declarations
- `ssz_script/lib/ssz.ssz` — 1 plugin index declarations

### Current `plugin index` Declarations

- `ssz_script/lib/alpha/lua.ssz:45` — `plugin index NewState(::) = "dll/lua.dll";`
- `ssz_script/lib/alpha/mesdialog.ssz:45` — `plugin index TazyuuCheck(:^/char:) = "dll/mesdialog.dll";`
- `ssz_script/lib/alpha/ogg.ssz:6` — `plugin index NewOggVorbis(::) = "dll/ogg.dll";`
- `ssz_script/lib/alpha/ogg.ssz:41` — `plugin index OggVorbisRead(:index, ^short:) = "dll/ogg.dll";`
- `ssz_script/lib/alpha/sdlplugin.ssz:124` — `plugin index SendWriteBGM(:^/short:) = "dll/sdlplugin.dll";`
- `ssz_script/lib/alpha/sdlplugin.ssz:825` — `plugin index AllocSurface(:int, int:) = "dll/sdlplugin.dll";`
- `ssz_script/lib/alpha/sdlplugin.ssz:831` — `plugin index IMGLoad(:^/char:) = "dll/sdlplugin.dll";`
- `ssz_script/lib/alpha/sdlplugin.ssz:842` — `plugin index CreatePaletteSurface(:ubyte=, uint=, int, int:) = "dll/sdlplugin.dll";`
- `ssz_script/lib/alpha/sdlplugin.ssz:847` — `plugin index SetColorKey(:index, int:) = "dll/sdlplugin.dll";`
- `ssz_script/lib/alpha/sdlplugin.ssz:867` — `plugin index OpenFont(:^/char, int:) = "dll/sdlplugin.dll";`
- `ssz_script/lib/file.ssz:20` — `plugin index Open(:^/char, ^/char:) = <dll/file.dll>;`
- `ssz_script/lib/file.ssz:26` — `plugin index Close(:index:) = <dll/file.dll>;`
- `ssz_script/lib/file.ssz:47` — `plugin index ReadAry(:index, ^_t, index:) = <dll/file.dll>;`
- `ssz_script/lib/file.ssz:58` — `plugin index WriteAry(:index, ^/_t, index:) = <dll/file.dll>;`
- `ssz_script/lib/regex.ssz:18` — `plugin index NewRegex(:^/char, bool, ^char=:) = <dll/regex.dll>;`
- `ssz_script/lib/socket.ssz:37` — `plugin index SocketAccept(:index, int, bool:) = <dll/socket.dll>;`
- `ssz_script/lib/socket.ssz:58` — `plugin index SocketRecvAry(:index=, ^_t, index:) = <dll/socket.dll>;`
- `ssz_script/lib/socket.ssz:69` — `plugin index SocketSendAry(:index=, ^/_t, index:) = <dll/socket.dll>;`
- `ssz_script/lib/sound.ssz:10` — `plugin index NewClient(::) = <dll/sound.dll>;`
- `ssz_script/lib/ssz.ssz:24` — `plugin index NewCompiler(::) = <dll/ssz.dll>;`

## Why a Second Bridge Is Needed

The existing `main/ssz/bridge.cpp` handles the **plugin ABI** boundary (Reference/PluginUtil → native). Converting the script layer requires a different bridge that handles the **script API surface** (libraries, classes, object lifetimes, arrays, string behavior, engine actions).

### Existing bridge (`main/ssz/bridge.cpp`)

```text
SSZ plugin ABI call
PluginUtil* + Reference
        ↓
main/ssz/bridge.cpp
        ↓
native C++ plugin function
std::wstring / std::vector / primitive / native buffer
```

### New compatibility layer (`main/ssz_native/`)

```text
Remaining SSZ runtime or translated/native subsystem
        ↓
ssz_native compatibility API
        ↓
native C++ implementations of former ssz_script behavior
```

Recommended location:

```text
main/ssz_native/
  ssz_native.hpp             // public umbrella header
  ssz_value.hpp              // SszString, SszBytes, SszArray, Index
  ssz_runtime_facade.hpp     // temporary API for unconverted SSZ
  file_service.hpp/.cpp
  string_service.hpp/.cpp
  math_service.hpp/.cpp
  table_service.hpp/.cpp
  regex_service.hpp/.cpp
  socket_service.hpp/.cpp
  ogg_service.hpp/.cpp
  sound_service.hpp/.cpp
  sdl_service.hpp/.cpp
  lua_bridge.hpp/.cpp
```

This is not the same as `bridge.cpp`. It models the **script-level API surface**.

### Why another bridge is needed during migration

If we convert one SSZ module at a time, some code will still run in the SSZ runtime while some behavior moves to C++. The compatibility layer avoids a big-bang rewrite and lets native C++ modules call equivalent services without depending on `Reference`, SSZ parser internals, or old plugin signatures.

**Final target:**

```text
Lua / engine / native gameplay code
        ↓
native C++ services
        ↓
no SSZ runtime dependency for converted systems
```

## Recommended Migration Strategy

Use a **strangler-fig migration**:

1. Freeze SSZ behavior with **trace captures** of representative input files.
2. Build native equivalents module-by-module.
3. Route selected calls to native C++ via the compatibility layer.
4. Keep SSZ runtime only for unconverted modules.
5. Delete SSZ modules only after native parity is proven.

**Do not** directly convert all `.ssz` syntax into C++ in one step. The codebase has ~40k lines of SSZ scripts with significant object-style logic. A source-to-source compiler can be useful later, but manual subsystem migration is safer first.

## Phase 0 — Research And Architecture Lock

- [ ] Document SSZ language features used in this repo:
  - [ ] `lib x = <...>` imports.
  - [ ] `plugin index` external/native calls.
  - [ ] `public &Type` object/class style declarations.
  - [ ] pointer/reference-like types: `^/char`, `^/_t`, `^short`, `index`, arrays.
  - [ ] template-like calls such as `readAry!ubyte?` / `writeAry!ubyte?`.
  - [ ] ownership conventions for object wrappers like `File`, `Regex`, `OggVorbis`, `Client`.
- [ ] Produce an SSZ dependency graph from all `lib ... = <...>` statements.
- [ ] Produce a symbol manifest for every public type/function/constant in `ssz_script/`.
- [ ] Decide whether native code naming should mirror SSZ names or use C++ domain names with adapter aliases.
- [ ] Choose namespace convention: `ikemen::ssz_native`.
- [ ] Add build target for native SSZ conversion files (`SSZ_NATIVE_SRCS` in Makefile).
- [ ] **NEW: Capture SSZ trace logs** for representative inputs (e.g., one `.def`, one stage, one character) before any conversion. These become the parity baseline.

## Phase 1 — Native Foundation Libraries

These are the safest initial conversion targets because they have limited engine state and can be unit-tested.

### 1. `ssz_script/lib/consts.ssz`

- [x] Convert constants to C++ `constexpr` values.
- [x] Place in `main/ssz_native/consts.hpp`:
  - `Signed<T>` / `Unsigned<T>` template structs with `MIN`/`MAX`
  - Type aliases: `byte_t`, `short_t`, `int_t`, `long_t`, `ubyte_t`, `ushort_t`, `uint_t`, `ulong_t`, `char_t`, `index_t`
  - Sentinel shorthand: `SENTINEL_MIN`, `SENTINEL_MAX`, `SENTINEL_UMAX`

### 2. `ssz_script/lib/math.ssz`

- [x] Convert pure functions to C++.
- [x] Call native math plugin functions via C standard library directly (sin, cos, tan, etc.).
- [x] Park-Miller LCG PRNG matching SSZ's random/srand/rand/randI/randF with module-level seed.
- [x] Utility templates: min, max, inRange, limMax, limMin, limRange, swap.
- [x] 293 tests (constants, wrappers, PRNG determinism, rand/randI/randF range, utility templates).

### 3. `ssz_script/lib/string.ssz`

- [ ] Create string utility library based on `std::wstring` and UTF conversion utilities.
- [ ] Keep behavior-compatible wrappers for SSZ string conventions.
- [ ] Document every encoding conversion path.
- [ ] **NEW: Audit Lua usage** — `lua_script/` calls into `lua_script/lib/string.ssz`? If so, ensure native C++ service exposes the same Lua-callable names.

### 4. `ssz_script/lib/table.ssz`

- [ ] Decide native representation: `using SszTable = std::vector<SszValue>` or domain-specific vectors/maps.
- [ ] Convert only functions actually used by engine scripts first.

### 5. `ssz_script/lib/base64.ssz`, `md5.ssz`, `arcfour.ssz`

- [ ] Prefer tested C++ implementations or existing project code if present.
- [ ] Add known-answer tests.
- [ ] Avoid changing serialized output formats.

## Phase 2 — Plugin Wrapper Libraries

These SSZ files mostly wrap native plugin calls and object lifetimes. The underlying `main/*.cpp` plugin implementations are already native (Phase 4 of `TODO.md` is complete). Now we replace the *SSZ-side* wrappers that translate the plugin ABI into SSZ object semantics.

**Convention:** Native service classes use RAII. SSZ object-style `&file.File` and friends map to `ikemen::ssz_native::FileHandle` etc. Each SSZ file remains loadable until *all* its callers are native.

### `ssz_script/lib/file.ssz`

- [x] Create `main/ssz_native/file_service.hpp/.cpp`.
- [x] Map `file.File` object to a RAII C++ `FileHandle` class.
- [x] Preserve `Open(:^/char, ^/char:)` argument behavior: mode + filename ordering.
- [x] `main/file/file.cpp` is already native, so `FileHandle` calls it directly — no plugin ABI in the new path.
- [ ] **Compatibility shim** — keep an `extern "C"` SSZ-callable wrapper in `bridge.cpp` that instantiates `FileHandle` and routes the SSZ object's methods to it. The shim is removed only when no SSZ script uses `&file.File` directly.

### `ssz_script/lib/regex.ssz`

- [x] Native RAII class wrapping `std::wregex` (or `boost::wregex` on Linux).
- [x] `search()` returns all capture groups from first match (matching SSZ RegexSearch behavior).
- [x] `search_all()` returns all non-overlapping full-match substrings.
- [x] `search_matches()` returns absolute position/length pairs.
- [x] `compile()` with `case_insensitive` flag; error message on invalid pattern.
- [x] Free function `compile()` returning `pair<Regex, wstring>`.
- [x] 18 tests (compile, search groups, search_all, case insensitive, error, move).

### `ssz_script/lib/socket.ssz`

- [x] Native RAII `SocketHandle` wrapping `SOCKET` handle.
- [x] connect (host, port, timeout, nodelay), listen (port, backlog, ipv4).
- [x] accept (returns new SocketHandle), close.
- [x] send/recv (scalar), send_array/recv_array (buffer).
- [x] Move semantics, double-close safety.
- [x] Calls native socket plugin functions directly (main/socket/socket.cpp).
- [x] 7 tests (construction, move, move-assign, double-close).

### `ssz_script/lib/sound.ssz`

- [x] Native RAII `AudioClient` wrapping opaque `Client*` handle.
- [x] Constructor calls `NewClient()`, destructor calls `DeleteClient()`.
- [x] start, stop, buffer_ready, set_buffer.
- [x] Move semantics, safety checks on null client.
- [x] 5 tests (construction, move, move-assign, no-crash on start/stop/buffer_ready).

### `ssz_script/lib/alpha/ogg.ssz`

- [ ] RAII wrapper for OggVorbis object.
- [ ] Preserve PCM total/rate/channel/read/seek semantics.

### `ssz_script/lib/alpha/sdlplugin.ssz` and `sdlevent.ssz`

- [ ] Do this late; it contains many calls and SDL object lifetimes.
- [ ] Create explicit native wrappers for SDL surface, texture, font, joystick/event types.
- [ ] Confirm every destructor/free path.

### `ssz_script/lib/alpha/lua.ssz`

- [ ] Defer until Lua/SSZ boundary is understood.
- [ ] Decide whether Lua talks directly to native C++ or through a retained runtime facade.
- [ ] **NEW:** Trace every Lua function that depends on this SSZ file. Without this audit we risk silently breaking Lua gameplay.

### `ssz_script/lib/alpha/mesdialog.ssz`

- [ ] Same as sdlplugin: RAII wrappers, late conversion.

## Phase 3 — Core Engine SSZ Modules

These files contain game/system behavior and should be converted after foundation libraries are native.

Recommended order (smallest first):

1. `ssz_script/ssz/share.ssz` — 371 lines
2. `ssz_script/ssz/system.ssz` — 427 lines
3. `ssz_script/ssz/debug-script.ssz` — 296 lines
4. `ssz_script/ssz/common.ssz` — 1199 lines
5. `ssz_script/ssz/loader.ssz`
6. `ssz_script/ssz/script.ssz` — 2216 lines
7. `ssz_script/ssz/system-script.ssz` — 2403 lines
8. `ssz_script/ssz/trigger-script.ssz` — 1633 lines
9. `ssz_script/ssz/statebuilder.ssz` — 9334 lines (last)

`statebuilder.ssz` is the largest file and should be converted only after smaller modules establish patterns.

## Phase 4 — Gameplay Resource Modules

Convert modules that represent MUGEN/Ikemen resource formats and runtime structures:

- [ ] `ssz_script/ssz/action.ssz`
- [ ] `ssz_script/ssz/bg.ssz`
- [ ] `ssz_script/ssz/char.ssz`
- [ ] `ssz_script/ssz/command.ssz`
- [ ] `ssz_script/ssz/fight.ssz`
- [ ] `ssz_script/ssz/fighting.ssz`
- [ ] `ssz_script/ssz/font.ssz`
- [ ] `ssz_script/ssz/sff.ssz`
- [ ] `ssz_script/ssz/sound.ssz`
- [ ] `ssz_script/ssz/stage.ssz`
- [ ] `ssz_script/ssz/video.ssz`

Recommended native structure:

```text
main/game/
  action.hpp/.cpp
  background.hpp/.cpp
  character.hpp/.cpp
  command.hpp/.cpp
  fight.hpp/.cpp
  font.hpp/.cpp
  sff.hpp/.cpp
  stage.hpp/.cpp
  video.hpp/.cpp
```

Keep file-format parsers deterministic and testable.

## Phase 5 — Save/Config SSZ Files

Files:

- [ ] `ssz_script/save/config.ssz`
- [ ] `ssz_script/save/configNet.ssz`

Approach:

- [ ] Convert static config defaults into C++ structs.
- [ ] Preserve serialized format if users already have config files.
- [ ] If moving to JSON/Lua config, write a migration loader.

## Proposed C++ Compatibility Types

Create `main/ssz_native/ssz_value.hpp`:

```cpp
namespace ikemen::ssz_native {

using Index = intptr_t;

struct SszString {
    std::wstring value;
};

struct SszBytes {
    std::vector<std::uint8_t> data;
};

template<class T>
using SszArray = std::vector<T>;

class RuntimeFacade {
public:
    // Temporary API for native modules that still need to call unconverted SSZ behavior.
};

} // namespace ikemen::ssz_native
```

Rules:

- Do not expose `Reference` outside bridge/runtime boundary code.
- Do not use `PluginUtil` in new native modules.
- Use RAII for every handle/object.
- Use explicit conversion functions at boundaries only.
- **NEW:** All `Index` parameters in native APIs that previously held an `SSZ index handle` must use a typed wrapper (e.g. `class FileIndex;` or `using FileIndex = SszObjectId<...>`) to prevent passing a socket ID where a file ID is expected. The `intptr_t` alias is too loose.

## Bridge Architecture During Conversion

### Boundary A — Plugin ABI Bridge

File: `main/ssz/bridge.cpp` (already complete)

Responsibility: SSZ plugin ABI ⇄ native plugin implementation.

Keep it narrow. It should not contain gameplay logic.

### Boundary B — SSZ Native Compatibility Layer

Files: `main/ssz_native/*.hpp/.cpp`

Responsibility: former `ssz_script` behavior ⇄ native C++ engine subsystems.

This layer may temporarily expose SSZ-like names/classes so converted and unconverted code can coexist. **Each class added here is a candidate for removal** once the calling SSZ scripts are themselves converted.

### Boundary C — Lua/API Bridge

Potential files: `main/lua_native/*.hpp/.cpp`

Responsibility: Lua scripts ⇄ native C++ engine API.

Only create this when converting SSZ modules changes functions consumed from Lua. Trace Lua ↔ SSZ call sites first.

## Build-System Plan

- [ ] Add `SSZ_NATIVE_SRCS` block to `Makefile`.
- [ ] Compile `main/ssz_native/*.cpp` into the app.
- [ ] Keep `ssz_script/` packaged until replacement is complete.
- [ ] Add a compile flag: `-DIKEMEN_USE_NATIVE_SSZ=1`.
- [ ] Add per-module feature flags so we can enable native replacements gradually:

```cpp
IKEMEN_NATIVE_FILE_LIB
IKEMEN_NATIVE_STRING_LIB
IKEMEN_NATIVE_STATEBUILDER
```

- [ ] **NEW:** Each `SSZ_FunctionEntry` registration in `*_static.hpp` should be guarded by `#ifdef IKEMEN_NATIVE_<MODULE>` so a module can be flipped off at compile time without code changes.
- [ ] **NEW:** Add a `native_manifest` target that prints which native modules are currently active at build time, for easier diagnosis.

## Testing Plan

### Static Tests

- [ ] Generate symbol manifest from SSZ scripts.
- [ ] Compare native manifest against SSZ manifest.
- [ ] Verify all old script entry names still exist or have explicit migration notes.
- [ ] **NEW:** Reject PRs that remove a public SSZ symbol without a corresponding native replacement (or an explicit deprecation note).

### Unit Tests

- [ ] String utilities.
- [ ] Math utilities (already covered — 21 tests pass).
- [ ] Table utilities.
- [ ] File read/write/find operations (already covered — 40 tests pass).
- [ ] Regex behavior.
- [ ] Socket buffer behavior with mocked socket where possible.

### SSZ-Native Parity Tests

- [ ] **NEW:** For each converted module, run the same input through the SSZ version and the native version and compare outputs. Save the SSZ output once at the start of conversion and use it as the gold standard.

### Golden-File Tests

- [ ] Load representative `.def`, `.cmd`, `.air`, `.sff`, and storyboard files.
- [ ] Compare parsed native structures against SSZ output traces.

### Runtime Smoke Tests

- [ ] Boot engine to title screen.
- [ ] Load default config.
- [ ] Load one character.
- [ ] Load one stage.
- [ ] Enter character select.
- [ ] Start match.
- [ ] Play audio.
- [ ] Run replay path if available.

## First Implementation Step Recommended

Start with `ssz_script/lib/file.ssz` because it already aligns with the current native ABI migration of `main/file/file.cpp`.

1. ~~Finish converting every `main/file/file.cpp` function to native ABI.~~ (DONE)
2. ~~Expand `main/ssz/bridge.cpp` wrappers for all file plugin calls.~~ (DONE — see `bridge.cpp:194-308`)
3. ~~Add `main/ssz_native/file_service.hpp/.cpp` RAII API.~~ (DONE)
4. ~~Mirror `file.ssz` object behavior in C++.~~ (DONE)
5. ~~Add tests for:~~ (DONE — 11 tests, all pass)
   - open/close
   - read/write array
   - save/load ASCII text
   - find/findDir
   - create/remove directory
6. Leave `ssz_script/lib/file.ssz` in place until native callers prove parity.
7. **NEW:** After step 6, audit every SSZ file that imports `lib/file.ssz` and rewrite those imports to call `FileHandle` directly.

## Exit Criteria

### Per-module

An SSZ module can be considered converted only when:

- [ ] Native C++ implementation exists in `main/ssz_native/` or `main/game/`.
- [ ] Old SSZ behavior is covered by tests or trace comparison.
- [ ] No new code depends on its `.ssz` implementation.
- [ ] Remaining SSZ scripts either do not import it or import a compatibility stub.
- [ ] Runtime smoke tests pass.
- [ ] The module has a rollback flag if conversion is risky.

### Whole-migration

The SSZ script layer migration is complete when:

- [ ] All 45 SSZ files either have a native C++ replacement or are explicit stubs.
- [ ] No SSZ runtime invocation is required during a normal engine boot.
- [ ] The compatibility layer (`ssz_native`) is empty or contains only stubs.
- [ ] `main/ssz/bridge.cpp` is empty (or only contains `Run` which the SSZ runtime requires for re-entrant compiles).
- [ ] `make CONFIG=Debug` builds with no warnings about unconverted modules.

## Risk Register

- **Hidden language semantics**: SSZ syntax and object behavior may not map 1:1 to C++. *Mitigation: parity tests per module.*
- **Ownership bugs**: SSZ `index` handles need RAII equivalents. *Mitigation: typed wrappers, not raw `intptr_t`.*
- **Encoding bugs**: `^/char`, UTF-8, local codepage, and wide strings need explicit treatment. *Mitigation: dedicated `string_service` with audit.*
- **Ordering bugs**: Resource-loading order may depend on script side effects. *Mitigation: capture pre-conversion traces.*
- **Lua dependency bugs**: Lua scripts may depend on names initialized by SSZ scripts. *Mitigation: audit Lua ↔ SSZ call sites before converting each module.*
- **Big-file risk**: `statebuilder.ssz` and `char.ssz` should not be first conversions. *Mitigation: explicit ordering in Phase 3 and 4.*
- **Compiler/runtime interaction**: The SSZ runtime still loads and compiles unconverted SSZ files. If a converted module's name collides with an SSZ symbol, linker/static-registration order may break. *Mitigation: avoid name collisions, use namespace isolation.*
- **Test gap**: Native implementations of complex game logic (e.g., state machine in `statebuilder.ssz`) are nearly impossible to verify by hand. *Mitigation: parity tests + golden file tests + heavy unit tests for state transitions.*
- **Performance regression**: Native C++ may behave differently in hot paths (e.g., string allocation patterns). *Mitigation: profile before/after each module conversion.*

## Immediate TODO

- [x] ~~Finish file plugin native ABI conversion.~~ (Done — see `TODO.md` Phase 1)
- [x] ~~Create `main/ssz_native/` directory.~~ (Done)
- [x] ~~Create `ssz_value.hpp` with compatibility types.~~ (Done)
- [x] ~~Implement native `FileHandle` service.~~ (Done — `file_service.hpp/.cpp`)
  - `FileHandle` RAII class: open, close, read, readArray, write, writeArray, seek
  - Free functions: loadAsciiText, saveAsciiText, remove, move, copy, find, findDir, createDirectory, removeDirectory, setCurrentDir, getCurrentDir
  - Calls native `main/file/file.cpp` functions directly (no SSZ plugin ABI)
   - 23 smoke tests pass (85 total)
- [x] ~~Convert `ssz_script/lib/consts.ssz` to C++ `constexpr`.~~ (Done — `consts.hpp`)
- [x] ~~Convert `ssz_script/lib/math.ssz` to C++.~~ (Done — `math_service.hpp/.cpp`)
  - Wrappers for math functions via C standard library
  - Park-Miller LCG with module-level seed (srand/random/rand/randI/randF)
  - Utility templates (min, max, inRange, limMax, limMin, limRange, swap)
  - 293 tests (includes 100 each for rand, randI, randF)
- [x] ~~Convert `ssz_script/lib/regex.ssz` to C++.~~ (Done — `regex_service.hpp/.cpp`)
  - RAII `Regex` class wrapping std::wregex / boost::wregex
  - search (capture groups), search_all, search_matches
  - Free function compile()
  - 18 tests
- [x] ~~Convert `ssz_script/lib/socket.ssz` to C++.~~ (Done — `socket_service.hpp/.cpp`)
  - RAII `SocketHandle` class wrapping SOCKET handle
  - connect, listen, accept, send, recv, send_array, recv_array
  - Calls native socket plugin functions (main/socket/socket.cpp)
  - 7 tests
- [x] ~~Convert `ssz_script/lib/sound.ssz` to C++.~~ (Done — `sound_service.hpp/.cpp`)
  - RAII `AudioClient` class wrapping opaque Client pointer
  - start, stop, buffer_ready, set_buffer
  - Calls native sound plugin functions (main/sound/sound.cpp)
  - 5 tests
- [ ] Generate SSZ dependency graph.
- [ ] Generate public symbol manifest.
- [ ] Audit Lua ↔ SSZ call sites.
- [ ] Add build flag `IKEMEN_USE_NATIVE_SSZ`.
- [ ] Add per-module `#ifdef` guards in `*_static.hpp`.
- [ ] Capture pre-conversion trace logs for representative inputs.
- [ ] Add runtime trace mode around SSZ entry points before replacing them.

## Naming note

The directory name `ssz_native/` and the namespace `ikemen::ssz_native` are deliberately close to "SSZ" to make grep-based discovery easy. If the layer grows large, consider splitting into `ikemen::file`, `ikemen::string`, etc., with `ssz_native` only as the umbrella namespace. The risk register entry on compiler/runtime interaction makes clear isolation important.

## Open Questions

- **Q1: Should native modules link against the SSZ runtime at all?**
  Recommendation: No, except for `Run` (the entry point) and any `plugin index` calls that haven't been migrated. This gives a clean separation between migrated and un-migrated code.

- **Q2: How do we handle SSZ scripts that are *generated* (e.g., `data/select.def` is updated by `main.cpp`)?**
  Recommendation: Keep `updateCharInSelectDef` and `updateStageInSelectDef` in `main.cpp` since they predate SSZ. Don't migrate these.

- **Q3: When can we delete `ssz_script/`?**
  Recommendation: Only after Phase 5 (all gameplay modules + statebuilder) is fully native and parity tests pass. Until then, keep `ssz_script/` packaged.
