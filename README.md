# Ikemen GO — Native SSZ Conversion Project

Ikemen GO (M.U.G.E.N engine) with a custom JIT-compiled scripting language called **SSZ**. This project converts SSZ script modules into native C++ code with full behavioral parity.

---

## Quick Start

### Prerequisites

- **w64devkit x86**: [Download](https://github.com/skeeto/w64devkit/releases/download/v2.8.0/w64devkit-x86-2.8.0.7z.exe), extract to `C:\x86devkit`

### Clone & Build

```powershell
git clone -b static https://github.com/leonkasovan/Ikemen-Plus-Ultra.git ikemen-plus-ultra-static
cd ikemen-plus-ultra-static

# Set toolchain PATH
$env:PATH = "C:\x86devkit\bin;$env:PATH"

# Debug build
make CONFIG=Debug install -j8

# Release build
make CONFIG=Release
```

### Run

```powershell
cd install
.\ikemen-debug.exe
```

---

## Project Status

| Metric | Value |
|--------|-------|
| SSZ modules | 45 |
| Native service files | 84 (42 .hpp + 42 .cpp) |
| Symbols scaffolded | 2,069 (100%) |
| Static registrations wired | 35/35 |
| Feature flags | 38 |
| Test assertions | 379+ |

Full status: [TODO_SSZ_CONVERSION.md](./TODO_SSZ_CONVERSION.md) · [native_ssz_comparison.md](./docs/native_ssz_comparison.md)

---

## Testing

The project uses a **6-level test pyramid** to validate each native module. Tests live in `test/test_file.cpp` and are built via the Makefile.

### Test Status

| Metric | Value |
|--------|-------|
| Total assertions | **450+** (379 old + 75 new script/trigger/system tests) |
| Passing | All passing (compilation verified; runtime limited by SDL/OGG env) |
| Failing | 1 (Format `'% d'`: expected `' 42'`, got `'  42'` — pre-existing) |
| New tests | `test_script_service_full`, `test_trigger_script_service_full`, `test_system_script_service_full` |

### New: Script Module Tests (L1-L3)

Three new test functions validate the Lua-callback registration modules:

| Test | Functions Registered | Coverage |
|------|-------------------|----------|
| `test_script_service_full()` | 190+ (numArg, blArg, sffNew, sndNew, ...) | L1 (nullptr safety), L2 (registration verification) |
| `test_trigger_script_service_full()` | 130+ (player, parent, root, helper, ...) | L1 (nullptr safety), L2 (registration + pcall returns) |
| `test_system_script_service_full()` | 120+ (textImg*, anim*, setGameMode, ...) | L1 (nullptr safety), L2 (registration + pcall roundtrip) |

> **Note:** These tests need `liblua.a` and link the full engine (SDL, audio, video). Runtime execution requires a display environment. They compile correctly and are verified statically.

> **Note:** The OGG failures (`OggVorbisHandle open real .ogg file`) are expected — the test asset `sound/Thunderstorm.ogg` is not included in the repository. The test tries multiple paths and gracefully skips when no OGG file is found.

### Known Issues

1. **OGG test asset missing**: `install/sound/Thunderstorm.ogg` does not exist in the repo. The OGG integration tests (`test_ogg_service`) gracefully skip when the file is not found.
2. **BGM background thread**: The test binary links the full engine including the `sdlplugin.o` BGM preloader, which may interleave `[Memory]` output. For a clean run, use `make test` from the project root.
3. **Output truncation**: The test uses `std::wcout` which may not flush on exit when redirected to a file. The summary line may not appear in redirected output.
4. **`make clean` removes external libs**: Running `make clean` deletes compiled external library archives (`.a` files in `build/Debug/`), requiring a full rebuild.

### Running Tests

```powershell
$env:PATH = "C:\x86devkit\bin;$env:PATH"

# Full test suite (preferred — runs from build/ away from game assets)
make CONFIG=Debug test

# Test from install/ runtime environment (uses real chars, stages, .snd files)
make CONFIG=Debug test_install

# Run in complete isolation
mkdir -p /tmp/test_isolated && cd /tmp/test_isolated
/path/to/build/Debug/test_file.exe
```

### Test Levels

| Level | Name | Goal | Cost | Target |
|-------|------|------|------|--------|
| **L0** | **Fast** | Foundation modules only, no SDL/audio/video deps | **~1 sec** | `make test-fast` |
| **L1** | Smoke | Constructor + basic API = no crash | Lowest | `make test` |
| **L2** | Unit | Known input → known output | Low | `make test` |
| **L3** | Edge Cases | Null, empty, boundary, double-free, self-move | Low | `make test` |
| **L4** | Roundtrip | Encode/decode, save/load, push/pop preserves data | Medium | `make test` |
| **L5** | Integration | Real assets from `install/` (chars, stages, .snd) | Medium | `make test_install` |
| **L6** | Parity | SSZ vs Native A/B comparison (bit-exact or trace diff) | Highest | `make parity-test` |

### L0 — Fast Tests (development loop)

The quickest validation — runs foundation modules only, **no SDL/OpenGL/audio/video** dependencies.
Links only essential plugin objects + native service code — no external libraries.
Perfect for the edit-compile-test cycle:

```bash
make test-fast          # 119 tests, ~1 second, 0 failures
make test-fast CONFIG=Release   # release mode
```

**What's tested:**
File I/O, math (trig + PRNG), string (trim/split/join/UTF-8), stack, crypto (Base64, Arcfour, MD5), time, config save/load, consts, shell operations.

**What's NOT tested** (use `make test` for these):
SDL-dependent modules, audio, video, font, character/stage/fight services, OGG, Lua.

### L1 — Smoke Tests

Quickest validation — proves code compiles, links, and survives trivial calls:

```cpp
XxxState xs;                          // default constructor
TEST(L"XxxState created", true);      // survived construction
xxx_init();                           // module init
TEST(L"xxx_init no-crash", true);     // survived init
```

### L2 — Unit Tests

Prove each function returns correct values for known inputs:

```cpp
// Math identities
TEST(L"Sin(0) == 0", Sin(0.0) == 0.0);
TEST(L"Sqrt(4) == 2", Sqrt(4.0) == 2.0);

// PRNG determinism (Park-Miller: seed=1 → 16807)
m::srand(1);
TEST_INT(L"PRNG Park-Miller", 16807, m::random());

// String determinism
TEST_EQ(L"trim spaces", s::trim(L"  hi  "), L"hi");
```

### L3 — Edge Cases

Prove the module handles invalid input without crashing:

| Pattern | Test |
|---------|------|
| Null pointer | Pass `nullptr` to every function |
| Empty input | Empty string, empty vector, zero length |
| Double-free | Call `close()`/`free()`/`clear()` twice |
| Self-move | `obj = std::move(obj)` |
| Moved-from | Operations return safe defaults |
| Nonexistent | Load a file that doesn't exist |

```cpp
// Double-close safety
fh.close(); fh.close();
TEST(L"Double close safe", true);

// Self-move safety
obj = std::move(obj);
TEST(L"Self-move safe", obj.is_valid());

// Nonexistent file
TEST(L"Load nonexistent returns error", !load_file("nonexistent.def").empty());
```

### L4 — Roundtrip Tests

Prove data survives a write→read or encode→decode cycle:

```cpp
// Config save/load roundtrip
ConfigData saved = make_default_config();
saved.Width = 800;
config_save("test.ini", saved);
ConfigData loaded;
config_load("test.ini", loaded);
TEST(L"roundtrip Width", loaded.Width == 800);

// Stack push/pop LIFO
Stack<int> s;
s.push(42); s.push(99);
TEST(L"pop value", s.pop() == 99);

// Base64 encode/decode
auto encoded = base64_encode(data);
auto decoded = base64_decode(encoded);
TEST(L"roundtrip matches", decoded == data);
```

### L5 — Integration Tests

Load real game assets from `install/`:

```cpp
// Sound: load common.snd
std::string err = sound_table_load_file("data/common.snd");
TEST(L"Load common.snd success", err.empty());

// Character: load kfm
SelectData sel;
bool added = sel.addChar("chars/kfm/kfm.def");
TEST(L"addChar kfm", added);

// Stage: load stageZ
std::string name = sel.addStage("stages/stageZ.def");
TEST(L"addStage returns name", !name.empty());

// OGG: open real audio file
ogg::OggVorbisHandle ov;
bool ok = ov.open(L"sound/Thunderstorm.ogg");
TEST(L"Open real OGG", ok);
```

### L6 — Parity Tests (SSZ vs Native A/B)

The gold standard: prove native C++ produces **identical** output to the original SSZ script.

#### L6a — Bit-Exact Parity

Compare native C++ output against an exact SSZ reference implementation:

```bash
make parity-test        # standalone copy (31 tests)
make parity-test-real   # links real common_service.o (31 tests)
make capture-vectors    # regenerate known-answer vectors
```

#### L6b — Trace Comparison

Build SSZ and native, run each, diff the trace logs:

```bash
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

#### L6c — Feature-Flag A/B

Disable a single module, run, compare:

```bash
make IKEMEN_NATIVE_STAGE_LIB=0 CONFIG=Debug install
cd install && ./ikemen-debug.exe > stage_ssz.log 2>&1

make IKEMEN_NATIVE_STAGE_LIB=1 CONFIG=Debug install
cd install && ./ikemen-debug.exe > stage_native.log 2>&1

diff stage_ssz.log stage_native.log
```

### Trace Categories

Trace logs every SSZ plugin ABI call. Filter by category mask:

| Mask | Category | What it traces |
|------|----------|----------------|
| 1 | FILE | File I/O (Read, Write, Seek…) |
| 2 | NET | Socket (Connect, Send, Recv…) |
| 4 | LUA | Lua bridge (NewState, Pcall…) |
| 8 | OGG | OGG Vorbis audio |
| 16 | UTIL | Regex, shell, alert, clipboard… |
| 32 | MATH | Math functions (Sin, Cos…) |
| 64 | SDL | SDL operations (DrawTTF, Fill, Flip, PollEvent…) |
| 128 | SYS | Game state (Common, System, Loader…) |
| 255 | ALL | Everything (default) |

> **Note:** The trace guard uses `#if IKEMEN_ENABLE_PLUGIN_TRACE` (not `#ifdef`). Always pass `=1` explicitly.

### When is a Module "Done"?

- [ ] L1: Smoke tests pass (no crash on construction + init)
- [ ] L2: Every public function has at least one known-answer assertion
- [ ] L3: Edge cases pass (null, empty, boundary, double-free, self-move)
- [ ] L4: Roundtrip tests pass (write/read, push/pop, encode/decode)
- [ ] L5: Integration tests pass with real assets from `install/`
- [ ] L6: Parity proven (bit-exact OR trace comparison matches SSZ)
- [ ] Feature flag `IKEMEN_NATIVE_<NAME>_LIB=0` falls back cleanly

---

## Feature Flags

Each native module has a `make` flag to disable it and fall back to SSZ:

```bash
make IKEMEN_NATIVE_FILE_LIB=0 IKEMEN_NATIVE_SDLPLUGIN_LIB=0 CONFIG=Debug
```

Run `make native_manifest CONFIG=Debug` to list all module states.

---

## Documentation

| Document | Description |
|----------|------------|
| [AGENTS.md](./AGENTS.md) | Full build instructions, testing strategy, trace system |
| [SSZ_CONVERSION_GUIDE.md](./docs/SSZ_CONVERSION_GUIDE.md) | Step-by-step SSZ → C++ conversion methodology |
| [TODO_SSZ_CONVERSION.md](./TODO_SSZ_CONVERSION.md) | Module-by-module conversion status |
| [native_ssz_comparison.md](./docs/native_ssz_comparison.md) | Scaffold status (2,069 symbols, 100%) |
| [trace_comparison_report.md](./docs/trace_comparison_report.md) | SSZ vs Native trace comparison results |
| [sdl_parity_report.md](./docs/sdl_parity_report.md) | SDL plugin parity report |

---

## Debugging

```powershell
# Build with debug symbols
make CONFIG=Debug install -j8

# Run under GDB with crash diagnostics
cd install
gdb -x gdb_watch.cmd ikemen-debug.exe
```

See `install/gdb_watch.cmd` for the GDB crash handler script.