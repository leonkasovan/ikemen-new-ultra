# Native C++ ABI Migration TODO

Goal: convert all plugin implementations under `main/` from the old SSZ plugin ABI:

```cpp
ReturnType SSZ_STDCALL FunctionName(PluginUtil* pu, Reference ...)
```

to native C++ signatures that use normal C++ values such as `std::wstring`,
`std::vector<std::wstring>`, `std::string`, primitive values, and raw buffers
where those buffers are already native.

The SSZ runtime/static registration still expects the old ABI while the project
is migrated. During the transition, `main/ssz/bridge.cpp` owns old-ABI wrapper
functions. Each wrapper converts `PluginUtil*`/`Reference` arguments to native
C++ values, calls the native implementation, and converts return/output values
back to SSZ `Reference` only when required.

## Current Step Implemented

- [x] Created `main/ssz/bridge.hpp`.
- [x] Reworked `main/ssz/bridge.cpp` as the compatibility layer for converted
      functions.
- [x] Converted `main/file/file.cpp::Open` from:

```cpp
extern "C" intptr_t SSZ_STDCALL Open(PluginUtil* pu, Reference md, Reference fn)
```

to:

```cpp
intptr_t SSZ_STDCALL Open(const std::wstring& md, const std::wstring& fn)
```

- [x] Added an old-ABI wrapper in `main/ssz/bridge.cpp`:

```cpp
extern "C" intptr_t SSZ_STDCALL Open(PluginUtil* pu, Reference md, Reference fn)
```

that forwards to the native `Open(std::wstring md, std::wstring fn)`.

- [x] Added `$(SSZ)/bridge.cpp` to `MAIN_SRCS` in `Makefile`.
- [x] Converted `Run` in `ssz.cpp` to native `Run(const std::wstring&)`, with the
      old ABI `Run(PluginUtil*, Reference)` forwarding to it via bridge wrapper.

## Important Compatibility Notes

1. Keep the old ABI symbol name available until `file_static.hpp` and the SSZ
   runtime call sites are migrated. The bridge wrapper currently preserves this.
2. Do not remove `PluginUtil`/`Reference` from static registration headers yet;
   those headers describe the bridge/runtime boundary, not the native plugin
   implementation boundary.
3. Preserve parameter order from the existing implementation. For `Open`, the
   old implementation used `md` as file mode and `fn` as filename:

```cpp
_wfopen_s(&pFile, filename, mode);
```

The native implementation keeps this behavior as:

```cpp
_wfopen_s(&pFile, fn.c_str(), md.c_str());
```

## Step-by-Step Migration Plan

### Phase 1 — File plugin (`main/file/file.cpp`) ✅ COMPLETE

- [x] Convert `Open`.
- [x] Convert `FileClose`.
- [x] Convert `Read`.
- [x] Convert `ReadAry`.
- [x] Convert `Write`.
- [x] Convert `WriteAry`.
- [x] Convert `Seek`.
- [x] Convert `LoadAsciiText`.
- [x] Convert `SaveAsciiText`.
- [x] Convert `Delete`, `Move`, `Copy`.
- [x] Convert `Find`, `FindDir`.
- [x] Convert `CreateDir`, `RemoveDir`, `SetCurrentDir`, `GetCurrentDir`.
- [x] Updated `bridge.cpp` with file wrappers for every converted function.
- [x] Build verified — no compile errors, no duplicates.

### Phase 2 — Simple plugins

Convert plugins with minimal `Reference` ownership first:

- [x] `main/math/math.cpp`
- [x] `main/time/time.cpp`
- [x] `main/alert/alert.cpp`
- [x] `main/thread/thread.cpp`
- [x] `main/shell/shell.cpp`

### Phase 3 — Medium plugins

- [x] `main/regex/regex.cpp`: use `std::wstring`, `std::vector<std::wstring>` output.
- [x] `main/socket/socket.cpp`: separate native raw buffer handling from SSZ arrays.
- [x] `main/ogg/ogg.cpp`: mostly native object pointer and primitive arguments.
- [x] `main/sound/sound.cpp`: raw audio buffer wrappers.

### Phase 4 — Complex plugins ✅ COMPLETE

- [x] `main/mesdialog/mesdialog.cpp`
- [x] `main/lua/lua.cpp`
- [~] `main/sdlplugin/sdlplugin.cpp` — 60/62 functions use native C++ ABI.
      `RenderMugenZoom` and `RenderMugenShadow` intentionally keep `Reference`
      parameters because they operate directly on SSZ VM‑owned pixel buffers.
      Unused `typeid.h` and `pluginutil.hpp` includes removed.
- [x] `main/main.cpp` — converted to native ABI:
      - Removed `PluginUtil`, `Reference`, `typeid.h`, `pluginutil.hpp` usage.
      - `Run()` now takes `const std::wstring&` (native signature).
      - Script path extracted from `CommandLineString<WCHR>` (already wide).
      - Still requires `#include "arrayandref.hpp"` because the static-registration
        headers declare bridge symbols with `Reference` by value, and MinGW's
        `__stdcall` decoration needs the complete type to compute correct `@N` suffix.
        **Known fragility:** This depends on MinGW's specific `__stdcall` name-mangling
        behavior. If the project ever targets MSVC (which uses a different `@N`
        decoration scheme), this will need revisiting.
- [x] `main/ssz/ssz.cpp` — all 8 exported functions converted to native ABI:
      - `Run`, `MemMarkBefore`, `MemMarkAfter`: take `const std::wstring&`
      - `NewCompiler`, `DeleteCompiler`, `CompilerRun`: take/return `CompilerState*`
      - `CompilerCompile`, `CompilerCompileString`: return `std::wstring`, take `CompilerState*`
      - Bridge wrappers for all 8 preserved in `ssz.cpp` for `ssz_static.hpp` registration.

### Phase 5 — Cleanup

- [x] Decide on static registration approach — **Keep bridge wrappers.**
      Each `*_static.hpp` declares old ABI functions with `extern "C"` and
      registers their addresses via `(void*)FunctionName`. The linker resolves
      these to the bridge wrappers in `bridge.cpp` / `ssz.cpp`, which forward
      to the native implementations. A native dispatcher (centralized marshaling)
      was considered but rejected: it would require changing the SSZ runtime
      calling convention, adds risk, and the current approach is simple, explicit,
      and proven correct. The bridge wrappers are trivially verifiable per function.
- [x] Remove unnecessary `pluginutil.hpp`, `arrayandref.hpp`, and `typeid.h` includes
      from native plugin `.cpp` files where no longer used.
      - `sdlplugin.cpp`: removed `typeid.h` and `pluginutil.hpp` (Jun 2025).
      - All other converted plugins still legitimately need their includes
        (Linux compat, `DynamicRef`, `RegexMatchInfo`).
- [x] Fix `arrayandref.hpp` to include `typeid.h` itself (was relying on includer
      to provide `VOID_TYPEID`; now self-contained).
- [x] Run build and fix compile/link errors module-by-module.
- [x] Add regression smoke tests for file IO, directory operations, text loading.
      - Created `test/test_file.cpp` with 40 tests covering Open/Close/Read/Write,
        ReadAry/WriteAry, Seek, SaveAsciiText/LoadAsciiText, Delete, Move, Copy,
        CreateDir/RemoveDir, SetCurrentDir/GetCurrentDir, and Find.
      - Build and run via `make test` or `make CONFIG=Debug test`.
      - Links against the native `file.o` (not the bridge) to test the actual
        converted implementations.
      - Extended to test math (21 tests: sin/cos/tan/ln/exp/sqrt/ceil/floor/isfinite/isinf/isnan/log)
        and thread (ThreadDelay smoke test, 1 test). Now 62 tests total.

## Validation Checklist For Each Function ✅ VERIFIED

- [x] **Native implementation no longer accepts `PluginUtil*`** — grep-confirmed:
      All native `SSZ_STDCALL` function signatures use only native types.
      Existing `PluginUtil::wToA()`/`PluginUtil::aToW()` calls are Linux-only
      platform helpers, not ABI parameters.
- [x] **Native implementation no longer accepts `Reference`** — grep-confirmed:
      Zero `Reference` usage in any native `SSZ_STDCALL` function. The two
      sdlplugin functions (`RenderMugenZoom`, `RenderMugenShadow`) are
      documented intentional exceptions.
- [x] **Bridge wrapper exists for the old ABI symbol** — linker-confirmed:
      Successful build (`make CONFIG=Debug`) resolves all bridge symbols.
- [x] **Wrapper preserves parameter order** — spot-checked all bridge wrappers
      in `bridge.cpp` and `ssz.cpp`; order matches native implementations.
- [x] **Wrapper releases/deletes any temporary `Reference` it allocates** —
      pattern-confirmed: every bridge wrapper that creates a `Reference` output
      calls `dst->releaseanddelete()` before assigning the new value.
- [x] **Static plugin registration still resolves to exactly one old-ABI symbol** —
      linker-confirmed: no multiple-definition errors.
- [x] **No duplicate exported/linked symbols** — linker-confirmed: no
      `duplicate symbol` errors from any `*_static.hpp` registration.
