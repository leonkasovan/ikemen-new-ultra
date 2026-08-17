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
| 14 plugin sources | `main/*/` | 50–200 each |
| 14 static plugin headers | `main/*/*_plugin.hpp` | 30–240 each |

**External dependencies:** Lua 5.2.4, SDL2, SDL2_image, SDL2_ttf, SDL2_mixer, FLAC, libogg, libvorbis, Freetype, libpng, zlib, GLEW, VLC, PortAudio, OpenGL.

---

## SSZ Plugin Resolution

SSZ scripts declare plugin functions with a library reference that determines how
the function pointer is resolved at compile time (see `SDLLItem` in
`main/ssz/sourcetree.hpp`):

| Declaration | Meaning | Resolution path |
|---|---|---|
| `plugin uint TickCount(::) = <dll/time.dll>;` | Load the **real DLL** `dll/time.dll` from disk | `LoadLibraryW` / `GetProcAddress` — fails at compile time if the DLL is missing |
| `plugin uint TickCount(::) = <time>;` | Resolve from the **static plugin registry** | Lookup in `StaticPluginRegistry` (`main/ssz/static_plugin_registry.hpp`) — no DLL file required |

The deciding factor is whether the reference ends in `.dll`: a bare library name
(`<time>`) always goes through the static registry, while any `.dll` path
(`<dll/time.dll>`) is treated as a real DLL. The two forms are mutually
exclusive — a bare name never falls back to dynamic loading, and a `.dll` path
never consults the static registry.

### Registering a static plugin

Each statically-linked plugin ships a registration header that maps its exported
functions into the registry, e.g. `main/time/time_plugin.hpp` for the `time`
plugin. These headers are listed in `PLUGIN_HEADERS` in the `Makefile` (they are
included only by `main/main.cpp`, which calls every `*_plugin_register()` before
the SSZ compiler runs).

To use a static plugin from an SSZ script, declare it with the bare library name:

```ssz
// lib/time.ssz — resolves TickCount/UnixTime from main/time/time.cpp
plugin uint TickCount(::) = <time>;
```

The corresponding SSZ script libraries live in `ssz_script/lib/` (bare-name
declarations) — the `lib/*.ssz` and `lib/alpha/*.ssz` wrappers were migrated
away from `<dll/xxx.dll>` when the static plugin system replaced the DLLs.

---

## Native SSZ Libraries (C++)

SSZ **library modules** (`lib ... = <name>;` statements) can also be implemented
entirely in C++ instead of `.ssz` files. When a script writes

```ssz
lib time = <time>;
```

and no `lib/time.ssz` file exists, the compiler resolves `<time>` through the
**native library registry** (`main/ssz/native_lib.hpp`) and synthesizes a module
from C++ function pointers — no `.ssz` file is required. This is how the SSZ
script libraries are progressively converted to native code:

| Declaration | Resolution |
|---|---|
| `lib time = <time.ssz>;` | Read the script file `lib/time.ssz` |
| `lib time = <time>;` (no `.ssz`) | Native C++ library via `NativeLib::FindLibrary("time")` — falls back to a file read if no such native library is registered |

### Anatomy of a native library

Each native library lives in `ssz_script/lib/<name>.cpp` and follows the plugin
ABI (see `main/ssz/native_lib.hpp`):

```cpp
// ssz_script/lib/time.cpp
static uint32_t SSZ_STDCALL TickCount(PluginUtil*);   // first arg is always PluginUtil*
static int64_t  SSZ_STDCALL UnixTime(PluginUtil*);

extern "C" bool time_lib_register()
{
    NativeLib::NativeFunction funcs[] = {
        { "tickCount", "uint ()",        (void*)TickCount },   // name, SSZ signature, fn ptr
        { "unixTime",  "long ()",        (void*)UnixTime  },
    };
    NativeLib::NativeLibrary lib;
    lib.name = "time";
    for (auto& f : funcs) lib.functions.push_back(f);
    return NativeLib::RegisterLibrary(lib);
}
```

The signature strings describe the SSZ view of each function (e.g. `"long (long,
long)"` for `long div(long, long)`) and are tokenized into a plugin-typed
henshuu, so calls like `time.tickCount()` are type-checked and JIT-compiled
exactly like `plugin` calls. Native members accept both `(:` and plain `(` call
syntax.

ABI notes (all proven by the plugin bridges in `main/ssz/bridge.cpp`):

- **Arguments arrive reversed** — the last SSZ parameter is the first C++
  parameter (`open(file, arg, cdir, waitfor, active)` becomes
  `ShellLibOpen(pu, active, waitfor, cdir, arg, file)`).
- **32-bit SSZ args** (`int`/`uint`/`bool`/`float`/...) occupy the low 32 bits of
  an 8-byte slot with unspecified high bits — declare them `int32_t`/
  `uint32_t`/`float`, never `int64_t` (which would read garbage).
- Strings arrive as `Reference`; convert with `ikemen::ssz_bridge::refToWstring`.
- **Out-parameters** (`type=`): the C function receives a **pointer to the
  caller's slot** as the corresponding parameter (the same `~DAINYUU_TOKEN`
  convention the parser uses) — `^ubyte dest=` arrives as `Reference* dest`,
  `index i=` arrives as `int32_t* ip`.  Write the result in place through the
  pointer; the JIT copies it back to the caller's variable.  A native
  function whose SSZ declaration has `=` but that takes the value by copy
  silently fails to write back — e.g. `nextLine(index i=, ...)` must advance
  `*ip` past the newline or `splitLines`' loop never terminates.
- **String/array returns** (`^char`/`^/char`/`^ubyte`): return the address of a
  heap-allocated `Reference` (`sszrefnewfunc(sizeof(Reference))` + `init()`, then
  `PluginUtil::wstrToRef` for strings or `refnew(size,1)`+`memcpy` for byte
  arrays), or `0` for a null/empty result.  The JIT unpacks the returned
  struct's fields (pointer/position/length) into the temp-ref registers — see
  the TMPREF-return handling in the native-lib plugin branch of
  `jitcompiler.hpp` (`Hensuu`).  String params arrive reversed like everything
  else, so `find(ptn, str)` becomes `StrLibFind(pu, str, ptn)`.

A native library may also expose **module variables** (`NativeVariable`): they
are registered as ordinary SSZ module variables (e.g. `"public int"` for a
cross-module-visible one), backed by the module's variable frame.  A C++
function that needs state holds it internally (e.g. a `static`); the registered
variable is for SSZ-side interface parity.

### Wiring a new native library

1. Create `ssz_script/lib/<name>.cpp` with `extern "C" bool <name>_lib_register()`.
2. Add it to `NATIVE_LIB_SRCS` in the `Makefile`.
3. Declare and call `<name>_lib_register()` in `main/main.cpp` before the SSZ
   compiler runs (alongside the plugin registrations).
4. Change the consuming script to `lib <name> = <name>;` and keep the original
   `.ssz` aside (e.g. `time.ssz.bak`) for comparison.

Current native libraries: `time` (`ssz_script/lib/time.cpp`), `shell`
(`ssz_script/lib/shell.cpp`), `thread` (`ssz_script/lib/thread.cpp`), the
`math` PRNG core (`ssz_script/lib/math.cpp` — consumed via delegation from
`math.ssz`, which keeps the template functions in SSZ), the `string`
plain-function core (`ssz_script/lib/string.cpp` — consumed via delegation
from `string.ssz`, which keeps the templates, list-returning functions, and
`&Format` in SSZ), the `md5` hash functions and `&Md5` streaming methods
(`ssz_script/lib/md5.cpp`), `arcfour` (`ssz_script/lib/arcfour.cpp` — the
one-shot `arcfourEnc(^ubyte dest=, ...)` plus the `&Arcfour` streaming
methods), `file` (`ssz_script/lib/file.cpp` — the module functions and the
`&File` methods), `regex` (`ssz_script/lib/regex.cpp` — the `&Regex`
methods), `sound` (`ssz_script/lib/sound.cpp` — the `&Client` methods),
`ssz` (`ssz_script/lib/ssz.cpp` — memMarkBefore/memMarkAfter/run and the
`&Compiler` methods), `socket` (`ssz_script/lib/socket.cpp` — the
non-template `&Socket` methods), and `table` (`ssz_script/lib/table.cpp` —
the concrete `hash` function, delegated from `table.ssz`).

### Why the template libraries (`stack`, `table`, `alert`) stay in SSZ

The remaining SSZ libraries are template-bound and **cannot** be ported to
native code. This was investigated and verified empirically:

- **`alert.ssz`** is a 3-line wrapper whose only use of `_t` is
  `typeid(_t)` — a compile-time constant. The plugin call itself is fully
  concrete; there is no runtime logic to move to C++.
- **`stack.ssz`** (`&Stack<_t>`/`&Node<_t>`) and **`table.ssz`**
  (`&NameTable<_t>`/`&IntTable<int_t,_t>`) are pure template data
  structures: their *fields* are template-typed (`^_t data`, `&Table!_t,
  node_t? t`) and their methods take delegate parameters
  (`each`/`forEach`/`operate`), so no concrete core exists to extract.
- **Native `_t` signatures don't work.** Registering a native function with
  `_t` in its signature string (encoding `TYPE_TOKEN` in the plugin type)
  compiles the module, but every call site fails with a generic
  "Compilation Error.": `KansuuKata`/`TokenToTypeId` in the JIT have no
  `TYPE_TOKEN` resolution, and a native module's henshuu are synthesized
  once at import — never re-parsed per instantiation.
- **`^null` (DYNREF) can't store data.** DYNREF params/returns work as
  *transients* in native calls (a concrete ref converts via `refToDref`),
  but `^null` locals and `^null` struct fields both fail to compile — the
  JIT has no way to hold a DynamicRef in a frame slot. So a native
  stack/table core holding generic `DynamicRef` data is impossible too.

How templates *do* work in SSZ: when a template body is instantiated
(`probe!int?`), the concrete function's body is **re-parsed from source**
with the template param bound as `TYPE_TOKEN + <concrete type>`
(`BlockOpen`/`MakeTree` in `sourcetree.hpp`, ~line 5999). That is why the
`socket` templates (`send<_t>`/`recv<_t>`/…) work — their `plugin`
declarations sit *inside* the template body and are re-parsed per
instantiation with `_t` bound, then resolved through the static plugin
registry. The native-lib registry has no equivalent re-parse.

`consts.ssz` is also intentionally left in SSZ — it is pure type-system
sugar (`&Signed<_t>`, `&Unsigned<_t>`, `null<_t>`).

**Known broken function:** `decBase64<_t>` in `base64.ssz` fails to
compile at template instantiation (`*_t x = (*_t)0;` in the bit-packing
fallback, line ~70).  This is a pre-existing bug carried over from the
original `base64.ssz` — the game never uses base64, so it was never
noticed.  Only the working surface (`encBase64<_t>`, `uintToB64Char`,
`b64CharToUint`) is covered by tests.

### Regression test suite

`test/ssz/` holds permanent SSZ regression tests for the native library
conversions, each comparing its output against a frozen reference
(`test/ssz/<name>.expected`):

| Test | Library | Reference |
|---|---|---|
| `md5test` | `md5` | RFC 1321 vectors (empty, `"abc"`, fox phrase), streaming == one-shot |
| `arcfourtest` | `arcfour` | RFC 6229 vectors (`Key`/`Plaintext`, `Secret`/`Attack at dawn`), streaming == one-shot |
| `regextest` | `regex` | match groups, no-match, invalid pattern |
| `filetest` | `file` | save/load/copy/move/remove, dirs, int write/readAry round trip |
| `basetest` | `base64` | `encBase64` of `hello`/`Man`/`Ma`, char<->uint helpers |
| `mathtest` | `math` | seeded PRNG sequence (deterministic LCG), sqrt/floor/ceil/isnan/isinf |
| `tabletest` | `table` | `hash` values matching the original SSZ algorithm |
| `timetest` | `time` | unixTime range, tickCount monotonic |
| `threadtest` | `thread` | sleep duration >= requested |
| `sockettest` | `socket` | graceful connect failure, listen |
| `soundtest` | `sound` | `&Client` calls complete (results are device-dependent) |
| `ssztest` | `ssz` | compileString + run a snippet, memMark |

Run with `bash test/run_ssz_tests.sh` (defaults to
`install/ikemen-debug.exe`, then `build/Debug/ikemen-debug.exe`); the
runner needs the `ssz_script/lib/` tree installed next to the exe
(`make CONFIG=Debug install`) so `<name>.ssz` imports resolve.  Tests
run from `test/work/` (gitignored) and exit nonzero on any failure.

Note: `&Md5`/`&Arcfour`-style struct tests must not feed a raw digest
back through `md5str()` — `md5str` hashes its input, so `md5str(digest)`
re-hashes; use `toHex!ubyte?(digest)` to render a digest.  Test scripts
live under `test/ssz/`, not `install/lib/` (the exe resolves `lib/x.ssz`
relative to its own directory, so passing an absolute path to a
`test/ssz/x.ssz` script works; `install/lib/` is copied from
`ssz_script/lib/` by `make install`).

### Struct-method delegation (the `&X` pattern)

Stateful structs (`&File`, `&Md5`, `&Arcfour`, `&Regex`, `&Client`, `&Compiler`,
`&Socket`) stay defined in their `.ssz` as data containers — field
declarations, `new()` allocation, templates (`read<_t>`, `recv<_t>`, …), and
any whole-object manipulation (`&Socket.accept`) stay in SSZ — while their
non-template method bodies delegate to the native lib:

```ssz
lib fl = <file>;

public &File
{
  index fh = 0;
  public bool open(^/char fn, ^/char mode)
  {
    ret .fl.fileOpen(`fh=, fn, mode);   // field passed as an out-param
  }
}
```

The `&X` pattern is: the struct's fields are passed to the native functions
as out-params (`index fh=` arrives as a pointer to the caller's slot,
read-write; `^uint count=`/`^ubyte buf=` arrive as pointers to the Reference
slot, mutated in place), and the `lib <x> = <x>;` import must appear *above*
the struct in the `.ssz` so the method bodies can reference it.  Struct
methods that need the *static* plugin ABI (template methods, `&Socket.accept`)
keep their `plugin ... = <name>` declarations — the static plugin registry
and the native-lib registry coexist.  Verification for each conversion was
behavior parity against the previous SSZ implementation (byte-identical
digests for `&Md5`, byte-identical RC4 output for `&Arcfour`, identical
matches/groups for `&Regex`, identical seek/open/close for `&File`).

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

- `make install` copies `build/Debug/ikemen-debug.exe` to `install/ikemen-debug.exe`
- Lua scripts in `install/script/` are read at runtime — no rebuild needed for Lua-only changes
- C++ changes require rebuild + `make install`
- `2>&1` merges stderr into stdout so all output goes to the log file
- `-Encoding ascii` ensures the em-dash in log messages doesn't corrupt the file
