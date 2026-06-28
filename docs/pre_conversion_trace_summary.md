# Pre-Conversion Trace Log Summary

**Captured:** 2026-06-29
**Engine:** ikemen-debug.exe (Debug, x86_64)
**Trace mode:** `IKEMEN_ENABLE_PLUGIN_TRACE=1`
**Input:** Default startup (no command-line arguments)
**Duration:** ~8 seconds until timeout (engine waiting for display)

## Overview

- **Total trace entries:** 43,690
- **Unique functions called:** 28
- **Trace file size:** 618 KB
- **Functions called 0 times (not yet observed in startup):** 144 of 172 bridge wrappers

This trace captures the engine's initialization sequence through the SSZ plugin ABI bridge. It serves as a parity baseline for Phase 3+ conversion work — any native replacement must produce functionally equivalent behavior.

## Call Frequency

| Count | Function | Phase |
|-------|----------|-------|
| 42,308 | Read | File I/O |
| 403 | Register | Lua init |
| 227 | ToString | Lua bridge |
| 227 | IsString | Lua bridge |
| 186 | Seek | File I/O |
| 112 | ToRef | Lua bridge |
| 112 | GetTop | Lua bridge |
| 37 | PushRef | Lua bridge |
| 33 | ReadAry | File I/O |
| 5 | FileClose | File I/O |
| 3 | Open | File I/O |
| 2 | SetVolume | Audio |
| 1 | IsNumber, ToNumber, TickCount, UnixTime, LuaInit, NewState | Initialization |
| 1 | RendererInit, FullScreenExclusive, FullScreen, CursorShow, AspectRatio, SetOpacity, WindowType | Display |
| 1 | MemMarkBefore, RunFile, PushString | Memory/Script |

## Trace Files

| File | Size | Entries | Functions | Description |
|------|------|---------|-----------|-------------|
| `pre_conversion_trace.log` | 618 KB | ~43,690 | 28 | Startup-only (8s, killed before main menu) |
| `pre_conversion_trace_gameplay.log` | 4.9 MB | ~343,000 | 47 | With automated keypress navigation (55s) |

## Key Observations

### Startup Trace (28 functions)

1. **File I/O dominates** — 42,308 `Read` calls (96.8% of all entries). Most file operations happen during engine initialization (loading data files, SFF sprites, SND audio, etc.).

2. **Lua bridge is active** — `Register`, `ToString`, `IsString`, `ToRef`, `GetTop`, `PushRef` together account for ~1,118 calls. The SSZ-to-Lua bridge is actively used during init.

3. **Display init is minimal** — Only 7 display-related calls (RendererInit, FullScreen, WindowType, etc.).

4. **28 of 172 bridge wrappers exercised** — 144 wrappers (83%) never called in startup-only trace.

### Gameplay Trace (47 functions, new functions in bold)

1. **Rendering active** — `Flip` (11), `RenderFontBatch` (10), `SoftFill` (11) — engine rendering frames
2. **Input processing** — `PollEvent` (28), `JoystickButtonState` (3) — engine reacting to keystrokes
3. **Audio** — `SetSndBuf` (13) — audio buffers being filled
4. **Regex** — `RegexSearch` (3,277), `NewRegex` (40), `DeleteRegex` (80) — pattern matching for menus/data
5. **File system** — `Find` (66), `Open` (70), `FileClose` (139) — directory listing for character/stage selection
6. **String encoding** — `AsciiToLocal` (15), `DecodePNG8` (4) — data processing
7. **Not yet observed** — 125 functions still not called: `BlitSurface`, `RenderMugenZoom`, `RenderMugenGl*`, socket, ogg, sound client, mesdialog dialogs, etc. These require actual character selection + stage load + fight start.

## Baseline Use

To reproduce:
```powershell
# Startup trace (8s)
Set-Location install
make IKEMEN_ENABLE_PLUGIN_TRACE=1 CONFIG=Debug install -j8
.\ikemen-debug.exe 2>&1 | Out-File -FilePath trace_new.log -Encoding ascii
diff (cat docs/pre_conversion_trace.log) (cat trace_new.log)

# Gameplay trace (55s with automated keypresses)
Set-Location install
.\ikemen-debug.exe 2>&1 | Out-File -FilePath trace_new_gameplay.log -Encoding ascii
# Send enter/down/up/left/right/space repeatedly to navigate menus
diff (cat docs/pre_conversion_trace_gameplay.log) (cat trace_new_gameplay.log)
```

During Phase 3 conversion, compare traces against the partially-converted engine to verify behavioral parity.
