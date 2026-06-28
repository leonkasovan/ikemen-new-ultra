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
| SSZ native layer | `main/ssz_native/*` | 33 files, ~2,400 total |
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

- `make install` copies `build/Debug/ikemen-debug.exe` to `install/ikemen-debug.exe`
- Lua scripts in `install/script/` are read at runtime — no rebuild needed for Lua-only changes
- C++ changes require rebuild + `make install`
- `2>&1` merges stderr into stdout so all output goes to the log file
- `-Encoding ascii` ensures the em-dash in log messages doesn't corrupt the file
