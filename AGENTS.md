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
| 13 plugin sources | `main/*/` | 50–200 each |
| 13 static headers | `main/*_static.hpp` | 30–240 each |

**External dependencies:** Lua 5.2.4, SDL2, SDL2_image, SDL2_ttf, SDL2_mixer, FLAC, libogg, libvorbis, Freetype, libpng, zlib, GLEW, VLC, OpenGL.

---

## Build Instructions

### Windows — w64devkit (MinGW/GCC, recommended)

Toolchain: [w64devkit x86](https://github.com/skeeto/w64devkit/releases/download/v2.8.0/w64devkit-x86-2.8.0.7z.exe)
Toolset: GCC (MinGW), x86_64.
Uses the `x86.hpp` raw-byte emitter for code generation.

```powershell
# Prerequisites: Install w64devkit to C:\x86devkit
# No $env:PATH setup needed — just call make directly.

# Release build
make CONFIG=Release           # → build/Release/ikemen.exe

# Debug build (with install target)
make CONFIG=Debug install -j8 # → build/Debug/ikemen-debug.exe → install/ikemen-debug.exe

# Clean rebuild
make clean
```

- All 19 external libraries compiled from source (~800 source files) into 19 static archives
- **Note:** The Makefile sets `PATH` internally for the toolchain (`as`, `ld`) — no manual environment setup required
- Debug exe is copied to `install/ikemen-debug.exe` via `make install`; it also renames the build output with `-debug` suffix

### Linux (Makefile, experimental)

```bash
make CONFIG=Release
```

- Uses system `g++` with `-std=c++17`
- Architecture detection via `uname -m` (supports `-m32` for x86)
- Also builds Lua 5.2.4 as a static library

### Short path for compilation

PowerShell session stays in current dir, so no cd needed inside the sh command:

```powershell
& C:\x86devkit\bin\sh.exe --login -c 'cd C:/Projects/ikemen-new-ultra && make CONFIG=Debug install -j8'
```

### Build → Run → Capture log (Debug workflow)

```powershell
# 1. Build Debug + install (copies exe to install/)
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
- Runtime layout (what the exe actually reads): Lua scripts in `install/script/`, SSZ scripts in `install/ssz/`, config in `install/save/config.ssz` — sources live in `lua_script/script/`, `ssz_script/`, `ssz_script/save/` and are copied by `make install`; script/config-only changes just need re-copying (no rebuild)
- C++ changes require rebuild + `make install`
- `2>&1` merges stderr into stdout so all output goes to the log file
- `-Encoding ascii` ensures the em-dash in log messages doesn't corrupt the file

### Timed test run (auto-stop)

```sh
# Runs 15s then force-kills the exe directly (w64devkit coreutils timeout)
cd install && timeout 15 ./ikemen-debug.exe > ikemen-debug.log 2>&1
```

- Hard kill skips atexit handlers — no exit-time MEMORY/TIME RANKING reports in the log
- Need the ranking reports? Run without timeout and quit with ESC (clean exit prints them)

---

## Runtime Config — `ssz_script/save/config.ssz`

Runtime reads `install/save/config.ssz` (source: `ssz_script/save/config.ssz`, copied by `make install`). Edit the source for persistence, sed the `install/` copy for quick test runs.

**Renderer** (`const int Renderer`): `0` = SDL2/Software, `1` = OpenGL 2.1, `2` = OpenGL 3.3 (default), `3` = OpenGL 4.6, `4` = DirectX (D3D11/D3D9 via SDL2).

```sh
# Set renderer (works for both source and install copy)
sed -i 's/const int Renderer = .*;/const int Renderer = 0;/' install/save/config.ssz   # 0=SW 1=GL21 2=GL33 3=GL46 4=DX
```

**PerformanceMonitor** (`const bool PerformanceMonitor`): prints per-frame `[Perf] FPS/Frame/Sprites` lines + exit-time TIME/MEMORY rankings.

```sh
sed -i 's/const bool PerformanceMonitor = true;/const bool PerformanceMonitor = false;/' install/save/config.ssz   # or true
```

- Always restore `Renderer = 2` after renderer test runs
- Boot log line `[Init] Requested renderer: "X" -> Y` confirms which backend actually initialized

---

## Screenshot System (3 layers)

Call chain: **Lua** `f_screenShot()` → **SSZ** shim → **C++** `TakeScreenShot` → deferred capture at present boundary.

| Layer | File | What |
|---|---|---|
| C++ impl | `main/sdlplugin/sdlplugin.cpp` `TakeScreenShot` | Sets pending flag only (script runs mid-frame — no complete buffer exists yet) |
| C++ capture | `GlSwapBuffers` (GL: reads `GL_BACK` before swap) + `CapturePendingShotSDL` / `Flip` (SDL2/DirectX: `SDL_RenderReadPixels` after `RenderCopy`, before `RenderPresent`) | Actual grab; alpha forced to 255 (framebuffer alpha leaks blended-sprite values → transparent PNG if not) |
| C++ bridge | `main/ssz/bridge.cpp` `TakeScreenShot` | SSZ ABI → native |
| SSZ shim | `ssz_script/ssz/system-script.ssz` (~:864, registered ~:2319 `L.register("takeScreenShot", ...)`) | Pulls path from Lua stack, calls `.sdl.takeScreenShot` |
| SSZ plugin decl | `ssz_script/lib/alpha/sdlplugin.ssz` (~:926) | Plugin call wrapper |
| Lua trigger | `lua_script/script/common.lua` `f_screenShot()` | Builds filename, calls global `takeScreenShot(...)`; PRINTSCREEN hotkeys in `common.lua` (menus) + `match.lua` (`addHotkey`) |

- Output goes to `install/screenshots/` (`screenshotPath` in `screenpack.lua`)
- 4th path: VLC video screenshots inside `PlayVideo` (`libvlc_video_take_snapshot`) — requires `libvlc.dll` at runtime
- Present-path split (matters for capture timing): `fighting.ssz:674` calls `.sdl.GlSwapBuffers()` for Renderer 1–3, `.sdl.flip()` for Renderer 0/4 — both capture sites must stay in sync with `TakeScreenShot`'s pending flag
- Auto-screenshot test hook: add a frame counter + `takeScreenShot(...)` call at the top of `loop()` in `lua_script/script/match.lua` (engine invokes global Lua `loop()` every match frame from `fighting.ssz:664`); `cmdInput()` is NOT called during quick-match runs (menu/storyboard flows only); Lua `print()` lands in the run log
