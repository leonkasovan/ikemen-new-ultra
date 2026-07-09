## Project Overview

Ikemen GO (M.U.G.E.N engine) with a custom JIT-compiled scripting language called **SSZ**. The engine is a **static plugin architecture**: 14 subsystems register exported functions with the SSZ runtime.

| Component | Key Files | Lines |
|---|---|---|
| Entry point | `main/main.cpp` | 165 |
| SSZ JIT compiler | `main/ssz/jitcompiler.hpp` | 8,886 |
| SSZ source tree | `main/ssz/sourcetree.hpp` | 8,583 |
| SDL renderer plugin | `main/sdlplugin/sdlplugin.cpp` | 5,826 |
| x86 codegen backend | `main/ssz/x86.hpp` | 3,680 |
| Plugin registry | `main/ssz/static_plugin_registry.hpp` | 170 |
| Platform abstraction | `main/ssz/sszdef.h` | 176 |
| Type ID definitions | `main/ssz/typeid.h` | 35 |
| SSZ native layer | `main/ssz_native/*` | 84 files, ~10,300 total |
| 13 plugin sources | `main/*/` | 50–200 each |
| 13 static headers | `main/*_static.hpp` | 30–240 each |

**External dependencies:** Lua 5.2.4, SDL2, SDL2_image, SDL2_ttf, SDL2_mixer, FLAC, libogg, libvorbis, Freetype, libpng, zlib, GLEW, VLC, PortAudio, OpenGL.

---

## Build Instructions

### Windows — w64devkit (MinGW/GCC, recommended)

Toolchain: [w64devkit x86](https://github.com/skeeto/w64devkit/releases/download/v2.8.0/w64devkit-x86-2.8.0.7z.exe)
Toolset: GCC (MinGW), x86_64.
Uses the `x86.hpp` raw-byte emitter for code generation.

```powershell
# Prerequisites: Install w64devkit to C:\x86devkit

# Set toolchain PATH
$env:PATH = "C:\x86devkit\bin;$env:PATH"
cd C:\Projects\ikemen-plus-ultra-static

# Release build
make CONFIG=Release           # → build/Release/ikemen.exe

# Debug build (with install target)
make CONFIG=Debug install -j8 # → build/Debug/ikemen-debug.exe → install/ikemen-debug.exe

# Clean rebuild
make clean
```

- All 19 external libraries compiled from source (~800 source files) into 19 static archives
- **Note:** The Makefile sets `PATH` internally for `as` and `ld` — just having `g++.exe` in PATH is sufficient
- Debug exe is copied to `install/ikemen-debug.exe` via `make install`; it also renames the build output with `-debug` suffix

### Linux (Makefile, experimental)

```bash
make CONFIG=Release
```

- Uses system `g++` with `-std=c++17`
- Architecture detection via `uname -m` (supports `-m32` for x86)
- Also builds Lua 5.2.4 as a static library

### Short path for compilation

Pick one — PowerShell session stays in current dir, so no short path needed:

```powershell
$env:PATH = "C:\x86devkit\bin;$env:PATH"
cd C:\Projects\ikemen-plus-ultra-static
```

### Build → Run → Capture log (Debug workflow)

```powershell
# 1. Build Debug + install (copies exe to install/)
$env:PATH = "C:\x86devkit\bin;$env:PATH"
make CONFIG=Debug install -j8

# 2. Run from install/ dir, redirect stdout+stderr to log
Set-Location -LiteralPath "install"
.\ikemen-debug.exe 2>&1 | Out-File -FilePath "ikemen-debug.log" -Encoding ascii
Set-Location ..

# 3. Review the log
Get-Content -Path "install\ikemen-debug.log" -Tail 30   # last 30 lines
Get-Content -Path "install\ikemen-debug.log" | Select-String "PATTERN"  # grep
```

- `make install` copies `build/Release/ikemen.exe` to `install/ikemen.exe`
- Lua scripts in `install/script/` are read at runtime — no rebuild needed for Lua-only changes
- C++ changes require rebuild + `make install`
- `2>&1` merges stderr into stdout so all output goes to the log file
- `-Encoding ascii` ensures the em-dash in log messages doesn't corrupt the file

---

## Native SSZ Conversion

The project is in the process of converting SSZ scripts into native C++ code (`main/ssz_native/`). See [docs/SSZ_CONVERSION_GUIDE.md](./docs/SSZ_CONVERSION_GUIDE.md) for the full methodology.

### Feature flags

Each converted module has a `make` flag to disable it and fall back to the original SSZ script:

```bash
make IKEMEN_NATIVE_FILE_LIB=0 IKEMEN_NATIVE_SDLPLUGIN_LIB=0 CONFIG=Debug
```

Run `make native_manifest CONFIG=Debug` to list all module states.

### Trace system (categorized)

Build with trace enabled to log every SSZ plugin ABI call. Categories filter out noise:

```powershell
# Trace only SDL operations (render, input, display, BGM)
make IKEMEN_ENABLE_PLUGIN_TRACE=1 IKEMEN_TRACE_MASK=64 CONFIG=Debug -j8
.\build\Debug\ikemen-debug.exe 2>&1 | findstr "[TRACE]" > trace_sdl.log
```

| Mask | Category | What it traces |
|------|----------|----------------|
| 1    | FILE     | File I/O (Read, Write, Seek…) |
| 2    | NET      | Socket (Connect, Send, Recv…) |
| 4    | LUA      | Lua bridge (NewState, Pcall…) |
| 8    | OGG      | OGG Vorbis audio |
| 16   | UTIL     | Regex, shell, alert, clipboard… |
| 32   | MATH     | Math functions (Sin, Cos…) |
| 64   | SDL      | **SDL operations** (DrawTTF, Fill, Flip, PollEvent…) |
| 128  | SYS      | Game state (Common, System, Loader…) |
| 255  | ALL      | Everything (default) |

### Current conversion status

| Metric | Value |
|--------|-------|
| SSZ modules | 45 |
| Native service files | 84 (42 .hpp + 42 .cpp) |
| Static registrations wired | 36/36 |
| Stub-only modules remaining | 10 |
| Symbols scaffolded | 2,069 (100%) |

Full status: [TODO_SSZ_CONVERSION.md](./TODO_SSZ_CONVERSION.md) · [native_ssz_comparison.md](./docs/native_ssz_comparison.md) · [native_test_results.txt](./docs/native_test_results.txt)

### Tests

Run the native SSZ test suite from the project root (preferred):
```bash
make CONFIG=Debug test
```

For a **fast development loop** (no SDL/OpenGL/audio dependencies, ~1 second):
```bash
make CONFIG=Debug test-fast
```

To run from the real runtime environment (uses actual chars, stages, and .snd files in `install/`):
```bash
make CONFIG=Debug test_install
```

**Known limitation:** `test_install` links all engine objects including the BGM preloader thread. When `.snd` files are present in `install/`, the background thread's `[Memory]` output interleaves with test output and may crash before the full suite completes. For a clean run, use `make test` (runs from `build/` away from assets).

To run in complete isolation (no asset interference):
```bash
export PATH=/c/x86devkit/bin:$PATH
mkdir -p /tmp/test_isolated
cd /tmp/test_isolated
/path/to/build/Debug/test_file.exe
```

**Trace note:** The trace guard in `main/ssz_native/ssz_trace.hpp` uses `#if IKEMEN_ENABLE_PLUGIN_TRACE` (not `#ifdef`). Always pass an explicit value:
- `IKEMEN_ENABLE_PLUGIN_TRACE=1` → trace enabled
- `IKEMEN_ENABLE_PLUGIN_TRACE=0` or undefined → trace disabled

The test suite currently has **379 assertions, 2 expected failures** (OGG asset not found). The OGG integration tests gracefully skip when `sound/Thunderstorm.ogg` is not present. The **fast test suite** has **119 assertions, 0 failures** and completes in ~1 second.

**Known build gotchas:**
- `make clean` preserves external library archives (`.a` files) — use `make distclean` to remove everything including external libs
- `ccache` is auto-detected when in PATH — wraps the compiler for faster incremental rebuilds

---

## Testing Strategy — Effective Tests for Completed Modules

Every completed native SSZ module must be validated across **six test levels**, from cheapest to most expensive. A module is only "done" when it passes all applicable levels.

### Test Level Pyramid

```
         ┌──────────┐
         │ L6 Parity │  ← SSZ vs Native A/B (trace diff, bit-exact)
        ┌┴──────────┴┐
        │ L5 Integration│ ← Real assets, cross-module
       ┌┴──────────────┴┐
       │ L4 Roundtrip   │  ← Encode/decode, save/load, push/pop
      ┌┴────────────────┴┐
      │ L3 Edge Cases    │  ← null, empty, boundary, double-free, self-move
     ┌┴──────────────────┴┐
     │ L2 Unit Tests      │  ← Known input → known output
    ┌┴────────────────────┴┐
    │ L1 Smoke Tests       │  ← Constructor + basic API = no crash
   ┌┴──────────────────────┴┐
   │ L0 Fast Tests          │  ← Foundation modules, no SDL deps, ~1 second
   └───────────────────────┘
```

### L0 — Fast Tests (development loop)

**Goal:** Run the edit-compile-test cycle in under a second by testing only foundation modules that have no SDL/OpenGL/audio/video dependencies.

**Pattern:**
```bash
make test-fast          # 119 tests, ~1 sec, 0 failures
```

**What's covered:** File I/O, math (trig + PRNG), string operations, stack, crypto (Base64, Arcfour, MD5), time, config save/load, consts, regex, socket, shell.

**What's excluded** (use `make test` or higher): SDL-dependent modules, audio/video, font, OGG decoding, Lua, character/stage/fight/bg services.

**To add a new test to the fast suite:**
1. Add the test function to `test/test_fast.cpp` (NOT to `test/test_file.cpp`)
2. Add the service `.o` file to `TEST_FAST_OBJS` in the Makefile
3. Only include services that don't need external libraries (no SDL, no Lua, no libogg, etc.)

### L1 — Smoke Tests (always start here)

**Goal:** Prove the native C++ code compiles, links, and doesn't crash on trivial calls.

**Pattern:** Create a default-constructed state object, call `init()`, verify no segfault.

```cpp
static void test_xxx_service() {
    using namespace ikemen::ssz_native;
    XxxState xs;                          // default constructor
    TEST(L"XxxState created", true);      // survived construction
    xxx_init();                           // module init
    TEST(L"xxx_init no-crash", true);     // survived init
}
```

**Checklist:**
- [ ] Default-construct all public structs
- [ ] Call `xxx_init()` (if the module has one)
- [ ] Verify struct fields have expected defaults (match SSZ source)
- [ ] Call each public API function once with valid arguments

### L2 — Unit Tests (known-answer)

**Goal:** Prove each function returns the correct value for a known input.

**Pattern:** Hardcode expected results from the SSZ reference or mathematical identity.

```cpp
// Math: identity checks
TEST(L"Sin(0) == 0", Sin(0.0) == 0.0);
TEST(L"Sqrt(4) == 2", Sqrt(4.0) == 2.0);

// String: deterministic output
TEST_EQ(L"trim spaces", s::trim(L"  hi  "), L"hi");

// PRNG: deterministic with fixed seed
m::srand(12345);
int32_t a = m::random();
m::srand(12345);
int32_t b = m::random();
TEST(L"PRNG deterministic", a == b);

// Known Park-Miller sequence (seed=1 → 16807)
m::srand(1);
TEST_INT(L"PRNG Park-Miller", 16807, m::random());
```

**Checklist:**
- [ ] Every public function has at least one assertion with a known expected value
- [ ] Math functions: test identities (sin(0)=0, cos(0)=1, ln(1)=0)
- [ ] PRNG: test determinism (same seed → same sequence)
- [ ] String functions: test with ASCII and Unicode input

### L3 — Edge Cases (defensive)

**Goal:** Prove the implementation handles invalid/boundary input without crashing or corrupting state.

**Patterns to test for EVERY module:**

| Pattern | Test |
|---------|------|
| **Null pointer** | Pass `nullptr` to every function taking a pointer |
| **Empty input** | Empty string, empty vector, zero length |
| **Negative values** | `-1` for index, negative counts |
| **Out-of-bounds** | Index beyond array size |
| **Double-free** | Call `close()`/`free()`/`clear()` twice |
| **Self-move** | `obj = std::move(obj)` |
| **Moved-from state** | Operations on a moved-from object return safe defaults |
| **Boundary values** | `INT32_MIN`, `INT32_MAX`, `0`, `UINT32_MAX` |
| **Nonexistent resources** | Load a file that doesn't exist |

```cpp
// Double-close safety
fh.close();
fh.close();  // must not crash
TEST(L"Double close safe", true);

// Self-move safety
obj = std::move(obj);
TEST(L"Self-move safe", obj.is_valid());  // or whatever invariant

// Moved-from: operations return safe defaults
auto other = std::move(handle);
TEST(L"Moved-from start returns false", handle.start() == false);

// Nonexistent file
TEST(L"Load nonexistent returns error", !load_file("nonexistent.def").empty());

// Empty input
TEST(L"Empty string returns empty", s::trim(L"").empty());
```

**Checklist:**
- [ ] Every owned-resource type (FileHandle, SocketHandle, OggVorbisHandle, LuaState, AudioClient) tested for double-free, self-move, and moved-from operations
- [ ] Every function accepting a pointer tested with `nullptr`
- [ ] Every string function tested with empty string
- [ ] Every load/open function tested with nonexistent path
- [ ] Every lookup function tested with nonexistent key

### L4 — Roundtrip Tests (data preservation)

**Goal:** Prove that data survives a write→read or encode→decode cycle intact.

**Patterns:**

```cpp
// Save → Load roundtrip
ConfigData saved = make_default_config();
saved.Width = 800;
config_save("test.ini", saved);
ConfigData loaded;
config_load("test.ini", loaded);
TEST(L"roundtrip Width", loaded.Width == 800);

// Push → Pop roundtrip
Stack<int> s;
s.push(42); s.push(99);
TEST(L"pop value", s.pop() == 99);
TEST(L"pop value", s.pop() == 42);
TEST(L"empty after pops", s.empty());

// Encode → Decode roundtrip (Base64)
auto encoded = base64_encode(data);
auto decoded = base64_decode(encoded);
TEST(L"roundtrip matches", decoded == data);

// ShareData push/copy roundtrip
share_push(sd1);
ShareData sd2;
share_copy(sd2);
TEST(L"share roundtrip field", sd2.field == expected);

// UTF-8 roundtrip
auto utf8 = to_utf8(L"Hello, 世界!");
TEST_EQ(L"utf8 roundtrip", from_utf8(utf8), L"Hello, 世界!");
```

**Checklist:**
- [ ] File I/O: write→read preserves content
- [ ] Config: save→load preserves all fields
- [ ] Stack: push→pop is LIFO
- [ ] Crypto: encode→decode preserves data
- [ ] Encoding: UTF-8, Base64, hex roundtrip
- [ ] Share/CommonData: push→copy preserves all fields
- [ ] Table: set→get returns same value; remove→get returns null

### L5 — Integration Tests (real assets)

**Goal:** Prove the module works with the actual game assets in `install/`.

**Pattern:** Load real `.snd`, `.def`, `.cmd`, `.ssz` files from the install directory.

```cpp
// Sound: load a real .snd file and verify sounds are extracted
std::string err = sound_table_load_file("data/common.snd");
TEST(L"Load common.snd success", err.empty());

const WaveData* snd = sound_table_get_sound(0, 0);
TEST(L"Found at least one sound", snd != nullptr);
if (snd) {
    TEST(L"Sound has samples", !snd->wav.empty());
    TEST(L"Sound channels valid", snd->channels == 1 || snd->channels == 2);
}

// Character: add char from install/
SelectData sel;
bool added = sel.addChar("chars/kfm/kfm.def");
TEST(L"addChar kfm", added);
TEST(L"char name non-empty", !sel.charlist[0].name.empty());

// Stage: add stage from install/
std::string name = sel.addStage("stages/stageZ.def");
TEST(L"addStage returns name", !name.empty());

// OGG: open real audio file
ogg::OggVorbisHandle ov;
bool ok = ov.open(L"sound/Thunderstorm.ogg");
TEST(L"Open real OGG", ok);
if (ok) {
    TEST(L"pcm_total > 0", ov.pcm_total() > 0);
    TEST(L"channels valid", ov.channels() == 1 || ov.channels() == 2);
    int16_t buf[4096];
    TEST(L"read returns samples", ov.read(buf, 4096) > 0);
}
```

**Checklist:**
- [ ] Sound: load `data/common.snd`, verify sounds extracted
- [ ] Character: load `chars/kfm/kfm.def`, verify name/displayname parsed
- [ ] Stage: load `stages/stageZ.def` or `stages/kfm.def`
- [ ] OGG: open `sound/Thunderstorm.ogg`, read PCM samples
- [ ] Config: load/save from the real `install/` path

### L6 — Parity Tests (SSZ vs Native A/B)

**Goal:** Prove the native C++ implementation produces **identical** output to the original SSZ script. This is the gold standard — a module isn't "complete" until parity is proven.

#### L6a — Bit-Exact Parity (preferred for data-transform functions)

Compare native C++ output against a known reference implementation. Used for pure functions (math, data transforms, palette operations).

```cpp
// palfx_parity_test.cpp pattern:
// 1. Define an SSZ reference implementation (mirrors SSZ source exactly)
// 2. Call both the real native function AND the reference
// 3. Assert bit-exact equality on every output byte

static std::vector<uint32_t> ssz_reference(const PalFXData& palfx,
                                            const std::vector<uint32_t>& pal) {
    // ... exact mirror of SSZ source code ...
}

// In test:
auto native_result = palfx_transform_palette(palfx, pal, false);
auto ssz_result = ssz_reference(palfx, pal);
for (size_t i = 0; i < 256; i++) {
    TEST("Palette entry matches", native_result[i] == ssz_result[i]);
}
```

**Build and run:**
```bash
make parity-test        # standalone copy of transform (31 tests)
make parity-test-real   # links real common_service.o (31 tests)
make capture-vectors    # regenerate known-answer vectors
```

#### L6b — Trace Comparison (for runtime behavior)

Build both SSZ and native versions with `IKEMEN_ENABLE_PLUGIN_TRACE=1`, run each for a fixed time, capture the trace logs, then diff them. Every function call, argument, and return should match.

```bash
# Automated A/B comparison (from do_parity_test.sh):
make IKEMEN_ENABLE_PLUGIN_TRACE=1 IKEMEN_TRACE_MASK=254 \
    IKEMEN_USE_NATIVE_SSZ=0 CONFIG=Debug -j8 install
cd install && ./ikemen-debug.exe > trace_baseline.log 2>&1 &
sleep 15 && taskkill /f /im ikemen-debug.exe

make IKEMEN_ENABLE_PLUGIN_TRACE=1 IKEMEN_TRACE_MASK=254 \
    IKEMEN_USE_NATIVE_SSZ=1 CONFIG=Debug -j8 install
cd install && ./ikemen-debug.exe > trace_native.log 2>&1 &
sleep 15 && taskkill /f /im ikemen-debug.exe

diff -u install/trace_baseline.log install/trace_native.log
```

**Trace mask values for focused testing:**

| Mask | Category | Use when testing |
|------|----------|-----------------|
| 1 | FILE | File I/O module |
| 2 | NET | Socket module |
| 4 | LUA | Lua bridge |
| 8 | OGG | OGG audio |
| 16 | UTIL | Regex, shell, alert |
| 32 | MATH | Math module |
| 64 | SDL | SDL plugin |
| 128 | SYS | System, common, loader |
| 254 | ALL-FILE | Everything except noisy file I/O |
| 255 | ALL | Everything (default) |

#### L6c — Feature-Flag A/B (for gameplay modules)

Disable just the module under test, run a fixed scenario, capture output. Re-enable, re-run, compare. This isolates a single module for regression detection.

```bash
# A/B test a single module:
make IKEMEN_NATIVE_STAGE_LIB=0 CONFIG=Debug install  # SSZ baseline
cd install && ./ikemen-debug.exe > stage_ssz.log 2>&1

make IKEMEN_NATIVE_STAGE_LIB=1 CONFIG=Debug install  # Native
cd install && ./ikemen-debug.exe > stage_native.log 2>&1

diff stage_ssz.log stage_native.log
```

### When is a module "done"?

A module graduates from "stub" to "complete" when ALL of these are true:
- [ ] L0: Fast tests pass (if the module is a foundation service with no SDL deps)
- [ ] L1: Smoke tests pass (no crash on construction + init)
- [ ] L2: Every public function has at least one known-answer assertion
- [ ] L3: Edge cases pass (null, empty, boundary, double-free, self-move)
- [ ] L4: Roundtrip tests pass (if applicable: write/read, push/pop, encode/decode)
- [ ] L5: Integration tests pass with real assets from `install/`
- [ ] L6a or L6b: Parity proven (bit-exact OR trace comparison matches SSZ)
- [ ] Feature flag `IKEMEN_NATIVE_<NAME>_LIB=0` falls back cleanly to SSZ script

### Test file organization

All tests live in `test/test_file.cpp` as `static void test_xxx_service()` functions called from `main()`. Each test function follows this template:

```cpp
static void test_xxx_service()
{
    std::wcout << L"\n--- Xxx service ---" << std::endl;
    using namespace ikemen::ssz_native;

    // L1: Smoke — default construction
    XxxState xs;
    TEST(L"XxxState created", true);

    // L2: Unit — known-answer assertions
    TEST(L"xxx_func(5) == 25", xxx_func(5) == 25);

    // L3: Edge cases
    TEST(L"xxx_func(-1) returns safe default", xxx_func(-1) == 0);

    // L4: Roundtrip (if applicable)
    xxx_save(42);
    TEST(L"roundtrip", xxx_load() == 42);

    // L5: Integration (if applicable)
    TEST(L"Load real asset", xxx_load_file("data/real_asset.ext").empty() == false);
}
```

**To add a new test:**
1. Add `#include "ssz_native/xxx_service.hpp"` at top of `test/test_file.cpp`
2. Write a `static void test_xxx_service()` function following the 6-level pattern
3. Add `test_xxx_service();` call in `main()`
4. If the new service has a `.o` file, add it to `TEST_FILE_OBJS` in the Makefile
5. Run `make CONFIG=Debug test` and verify 0 failures

### Parity test: PalFX golden reference

The `test/palfx_parity_test.cpp` demonstrates the gold standard for parity testing:
- `test/palfx_known_vectors.hpp` — pre-computed known-answer vectors
- `test/palfx_parity_test.cpp` — compares native implementation against SSZ reference
- `test/palfx_stubs.cpp` — stubs for unreachable symbols (fill/softFill)
- `test/palfx_vector_capture.cpp` — captures new known-answer vectors

This pattern should be replicated for other data-transform functions (color operations, coordinate transforms, damage calculations) where bit-exact output is required.
